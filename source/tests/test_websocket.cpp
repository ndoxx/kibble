/**
 * @file test_websocket.cpp
 * @brief Catch2 unit / integration tests for kb::net WebSocket implementation.
 *
 * Architecture
 * ============
 * Most tests spin a real WebSocketAcceptor on 127.0.0.1 and connect to it
 * with a raw TCP socket that speaks the WebSocket wire protocol manually.
 * This lets us inject malformed frames, bad handshakes, and edge-case payloads
 * without needing an external process or a browser.
 *
 * Two threads are used per test that needs a live connection:
 *   - Server thread  : calls acceptor.accept() (blocking), then exercises the
 *                      WebSocketStream API under test.
 *   - Main thread    : acts as the "client", performs the HTTP upgrade by hand,
 *                      then sends/receives raw WebSocket frames.
 *
 * Helper: RawClient
 * -----------------
 * RawClient wraps a blocking POSIX/Winsock TCP socket. It can:
 *   - perform a valid RFC 6455 HTTP upgrade handshake,
 *   - send a raw (possibly intentionally malformed) WebSocket frame,
 *   - read raw bytes from the socket.
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kibble/net/error.h"
#include "kibble/net/websocket_acceptor.h"
#include "kibble/net/websocket_stream.h"
#include "kibble/string/base64.h"

// Platform socket primitives
#include "kibble/platform/platform.h"
#if defined(K_PLATFORM_LINUX)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using raw_socket_t = int;
static constexpr raw_socket_t k_bad_sock = -1;
inline void close_raw(raw_socket_t s)
{
    ::close(s);
}
#elif defined(K_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using raw_socket_t = SOCKET;
static constexpr raw_socket_t k_bad_sock = INVALID_SOCKET;
inline void close_raw(raw_socket_t s)
{
    ::closesocket(s);
}
#endif

#include <array>
#include <atomic>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

namespace
{

// ---- Port helpers --------------------------------------------------------

// Each test fixture grabs a unique port from this counter so that lingering
// TIME_WAIT sockets from an earlier test do not cause EADDRINUSE.
std::atomic<uint16_t> g_next_port{19001};
uint16_t next_port()
{
    return g_next_port.fetch_add(1, std::memory_order_relaxed);
}

// ---- Base64 for the handshake key ----------------------------------------

// Produce a valid 16-byte (base64-encoded to 24-char) Sec-WebSocket-Key.
std::string make_ws_key()
{
    const unsigned char raw[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
                                   0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    return kb::su::base64_encode(raw);
}

// ---- RawClient -----------------------------------------------------------

/**
 * @brief Blocking TCP client that speaks raw WebSocket frames.
 *
 * Wraps a POSIX / Winsock socket. All sends/receives are synchronous.
 * Intended only for tests - no error-recovery logic.
 */
class RawClient
{
public:
    explicit RawClient(uint16_t port)
    {
        sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock_ != k_bad_sock);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        REQUIRE(::connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    }

    ~RawClient()
    {
        if (sock_ != k_bad_sock)
        {
            close_raw(sock_);
        }
    }

    // Non-copyable
    RawClient(const RawClient&) = delete;
    RawClient& operator=(const RawClient&) = delete;

    // ---- Handshake -------------------------------------------------------

    /**
     * @brief Perform a valid RFC 6455 HTTP upgrade and verify the 101 response.
     * @param origin  Origin header value; empty -> omit the header entirely.
     */
    void do_handshake(const std::string& origin = "http://localhost")
    {
        ws_key_ = make_ws_key();
        std::string req = "GET / HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Key: " +
                          ws_key_ +
                          "\r\n"
                          "Sec-WebSocket-Version: 13\r\n";
        if (!origin.empty())
        {
            req += "Origin: " + origin + "\r\n";
        }
        req += "\r\n";

        send_raw(req.data(), req.size());
        const std::string resp = recv_until("\r\n\r\n");
        REQUIRE_THAT(resp, Catch::Matchers::ContainsSubstring("101 Switching Protocols"));
    }

    /**
     * @brief Send a custom HTTP upgrade request (for negative handshake tests).
     */
    void send_raw_http(const std::string& raw_request)
    {
        send_raw(raw_request.data(), raw_request.size());
    }

    // ---- Frame helpers ---------------------------------------------------

    /**
     * @brief Build and send one WebSocket frame (client-to-server, always masked).
     *
     * @param opcode  WebSocket opcode byte
     * @param payload Message payload (will be masked)
     * @param fin     Whether to set the FIN bit
     */
    void send_frame(uint8_t opcode, const std::string& payload, bool fin = true)
    {
        std::vector<char> frame;

        // Byte 0: FIN + opcode
        frame.push_back(static_cast<char>((fin ? 0x80u : 0x00u) | (opcode & 0x0Fu)));

        // Byte 1+: mask bit set, payload length
        const size_t len = payload.size();
        if (len < 126)
        {
            frame.push_back(static_cast<char>(0x80u | static_cast<uint8_t>(len)));
        }
        else if (len <= 0xFFFF)
        {
            frame.push_back(static_cast<char>(0x80u | 126u));
            frame.push_back(static_cast<char>((len >> 8) & 0xFF));
            frame.push_back(static_cast<char>(len & 0xFF));
        }
        else
        {
            frame.push_back(static_cast<char>(0x80u | 127u));
            for (int sh = 56; sh >= 0; sh -= 8)
            {
                frame.push_back(static_cast<char>((len >> sh) & 0xFF));
            }
        }

        // 4-byte masking key
        const std::array<uint8_t, 4> mask_key = {0x37, 0xfa, 0x21, 0x3d};
        for (auto b : mask_key)
        {
            frame.push_back(static_cast<char>(b));
        }

        // Masked payload
        for (size_t i = 0; i < payload.size(); ++i)
        {
            frame.push_back(static_cast<char>(static_cast<uint8_t>(payload[i]) ^ mask_key[i % 4]));
        }

        send_raw(frame.data(), frame.size());
    }

    /**
     * @brief Build and send a frame with the mask bit cleared (invalid: client
     *        frames must always be masked per RFC 6455 §5.3).  Used to test
     *        that the server does NOT reject unmasked frames - the spec says
     *        servers MUST close, but our implementation silently accepts them
     *        (common permissive behaviour). Adjust if your policy differs.
     */
    void send_unmasked_frame(uint8_t opcode, const std::string& payload)
    {
        std::vector<char> frame;
        frame.push_back(static_cast<char>(0x80u | (opcode & 0x0Fu)));
        const size_t len = payload.size();
        REQUIRE(len < 126);                      // keep it simple for tests
        frame.push_back(static_cast<char>(len)); // mask bit NOT set
        frame.insert(frame.end(), payload.begin(), payload.end());
        send_raw(frame.data(), frame.size());
    }

    /**
     * @brief Send a frame with one or more RSV bits set - must be rejected.
     */
    void send_rsv_frame(uint8_t opcode, uint8_t rsv_bits, const std::string& payload)
    {
        std::vector<char> frame;
        // FIN + RSV bits + opcode
        frame.push_back(static_cast<char>(0x80u | (rsv_bits & 0x70u) | (opcode & 0x0Fu)));
        const size_t len = payload.size();
        REQUIRE(len < 126);
        frame.push_back(static_cast<char>(0x80u | static_cast<uint8_t>(len)));
        const std::array<uint8_t, 4> mask_key = {0x11, 0x22, 0x33, 0x44};
        for (auto b : mask_key)
        {
            frame.push_back(static_cast<char>(b));
        }
        for (size_t i = 0; i < payload.size(); ++i)
        {
            frame.push_back(static_cast<char>(static_cast<uint8_t>(payload[i]) ^ mask_key[i % 4]));
        }
        send_raw(frame.data(), frame.size());
    }

    /**
     * @brief Read the next text frame the server sends and return its payload.
     *
     * This is a minimal reader: handles only unfragmented, unmasked server
     * frames with up to 16-bit payload length (sufficient for all test cases).
     */
    std::string recv_text_frame()
    {
        char header[2];
        recv_exact(header, 2);

        const uint8_t byte1 = static_cast<uint8_t>(header[1]);
        const uint8_t raw_len = byte1 & 0x7Fu;

        size_t pay_len = 0;
        if (raw_len < 126)
        {
            pay_len = raw_len;
        }
        else if (raw_len == 126)
        {
            char ext[2];
            recv_exact(ext, 2);
            pay_len = size_t((static_cast<uint8_t>(ext[0]) << 8u) | static_cast<uint8_t>(ext[1]));
        }
        else
        {
            // 64-bit length - not needed in our tests
            FAIL("recv_text_frame: 64-bit payload length not supported in this helper");
        }

        std::string payload(pay_len, '\0');
        if (pay_len > 0)
        {
            recv_exact(payload.data(), pay_len);
        }
        return payload;
    }

    /** @brief Read a close frame sent by the server; returns the status code. */
    uint16_t recv_close_frame()
    {
        char header[2];
        recv_exact(header, 2);
        // opcode should be 0x08 (close), FIN bit set
        REQUIRE((static_cast<uint8_t>(header[0]) & 0x0Fu) == 0x08u);
        const uint8_t pay_len = static_cast<uint8_t>(header[1]) & 0x7Fu;
        if (pay_len < 2)
        {
            return 0;
        }
        char body[2];
        recv_exact(body, 2);
        // Skip any reason string
        for (int i = 2; i < pay_len; ++i)
        {
            char c;
            recv_exact(&c, 1);
        }
        return uint16_t((static_cast<uint16_t>(static_cast<uint8_t>(body[0])) << 8u) | static_cast<uint8_t>(body[1]));
    }

    // ---- Low-level IO ----------------------------------------------------

    void send_raw(const char* data, size_t len)
    {
        size_t sent = 0;
        while (sent < len)
        {
            const auto n = ::send(sock_, data + sent, len - sent, 0);
            REQUIRE(n > 0);
            sent += static_cast<size_t>(n);
        }
    }

    void recv_exact(char* buf, size_t len)
    {
        size_t got = 0;
        while (got < len)
        {
            const auto n = ::recv(sock_, buf + got, len - got, 0);
            REQUIRE(n > 0);
            got += static_cast<size_t>(n);
        }
    }

    /** Read until `terminator` is found and return everything including it. */
    std::string recv_until(std::string_view terminator)
    {
        std::string buf;
        while (buf.find(terminator) == std::string::npos)
        {
            char c;
            const auto n = ::recv(sock_, &c, 1, 0);
            if (n <= 0)
            {
                break;
            }
            buf += c;
        }
        return buf;
    }

    /** Attempt to receive up to `max` bytes; returns however many arrived. */
    std::string recv_some(size_t max = 512)
    {
        std::string buf(max, '\0');
        const auto n = ::recv(sock_, buf.data(), max, 0);
        if (n <= 0)
        {
            return {};
        }
        buf.resize(static_cast<size_t>(n));
        return buf;
    }

private:
    raw_socket_t sock_{k_bad_sock};
    std::string ws_key_;
};

// ---- ServerFixture -------------------------------------------------------

/**
 * @brief Lightweight RAII fixture: starts a WebSocketAcceptor and exposes a
 *        helper that runs a server-side lambda in a background thread.
 *
 * Usage:
 * @code
 *   ServerFixture fix;
 *   auto server = fix.run_server([](std::unique_ptr<WebSocketStream> ws, ...) {
 *       // ... server-side test logic ...
 *   });
 *   RawClient client(fix.port());
 *   client.do_handshake();
 *   // ... client-side test logic ...
 *   server.get(); // re-throw any exception from the server thread
 * @endcode
 */
struct ServerFixture
{
    uint16_t port;
    kb::net::WebSocketAcceptor acceptor;

    ServerFixture() : port(next_port()), acceptor(port, "127.0.0.1")
    {
        auto r = acceptor.start();
        REQUIRE(r.has_value());
    }

    /**
     * @brief Accept one connection and run `fn(ws, warning)` in a thread.
     * Returns a future so the test can join and re-throw exceptions.
     */
    template <typename Fn>
    std::future<void> run_server(Fn&& fn)
    {
        return std::async(std::launch::async, [this, f = std::forward<Fn>(fn)]() mutable {
            kb::net::WSWarning warning;
            auto ws = acceptor.accept(&warning);
            f(std::move(ws), std::move(warning));
        });
    }
};

} // anonymous namespace

// ===========================================================================
// Section 1: Error types
// ===========================================================================

TEST_CASE("NetError carries code and context", "[error]")
{
    kb::net::NetError e{42, "test_op()"};
    const std::string msg = e.message();
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("test_op()"));
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("42"));
}

TEST_CASE("WSError::tcp wraps a NetError", "[error]")
{
    auto e = kb::net::WSError::tcp(kb::net::NetError{5, "connect()"});
    const std::string msg = e.message();
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("TCP error"));
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("connect()"));
}

TEST_CASE("WSError::protocol carries the description", "[error]")
{
    auto e = kb::net::WSError::protocol("non-zero RSV bits");
    const std::string msg = e.message();
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("WS protocol error"));
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("non-zero RSV bits"));
}

TEST_CASE("WSWarning empty / non-empty", "[error]")
{
    kb::net::WSWarning w;
    CHECK(w.empty());
    w.text = "oops";
    CHECK_FALSE(w.empty());
    CHECK_THAT(w.message(), Catch::Matchers::ContainsSubstring("oops"));
}

// ===========================================================================
// Section 2: HTTP upgrade handshake
// ===========================================================================

TEST_CASE("Valid handshake succeeds and produces a WebSocketStream", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto warning) {
        REQUIRE(ws_result.has_value());
        CHECK(warning.empty());
        // Send a greeting so the client can verify the stream is live.
        auto r = (*ws_result)->send_text("hello");
        CHECK(r.has_value());
    });

    RawClient client(fix.port);
    client.do_handshake("http://localhost");
    const auto greeting = client.recv_text_frame();
    CHECK(greeting == "hello");

    server.get();
}

TEST_CASE("Missing Upgrade header is rejected with a WSError", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE_FALSE(ws_result.has_value());
        CHECK_THAT(ws_result.error().message(), Catch::Matchers::ContainsSubstring("Upgrade"));
    });

    RawClient client(fix.port);
    // Deliberately omit the Upgrade header.
    client.send_raw_http("GET / HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Key: " +
                         make_ws_key() +
                         "\r\n"
                         "Sec-WebSocket-Version: 13\r\n"
                         "\r\n");
    // The server should send a 400 and close.
    const auto resp = client.recv_some();
    CHECK_THAT(resp, Catch::Matchers::ContainsSubstring("400"));

    server.get();
}

TEST_CASE("Missing Connection header is rejected", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE_FALSE(ws_result.has_value());
        CHECK_THAT(ws_result.error().message(), Catch::Matchers::ContainsSubstring("Connection"));
    });

    RawClient client(fix.port);
    client.send_raw_http("GET / HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Upgrade: websocket\r\n"
                         "Sec-WebSocket-Key: " +
                         make_ws_key() +
                         "\r\n"
                         "Sec-WebSocket-Version: 13\r\n"
                         "\r\n");
    client.recv_some();

    server.get();
}

TEST_CASE("Wrong Sec-WebSocket-Version is rejected", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE_FALSE(ws_result.has_value());
        CHECK_THAT(ws_result.error().message(), Catch::Matchers::ContainsSubstring("Version"));
    });

    RawClient client(fix.port);
    client.send_raw_http("GET / HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Key: " +
                         make_ws_key() +
                         "\r\n"
                         "Sec-WebSocket-Version: 8\r\n" // wrong version
                         "\r\n");
    client.recv_some();

    server.get();
}

TEST_CASE("Missing Sec-WebSocket-Key is rejected", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE_FALSE(ws_result.has_value());
        CHECK_THAT(ws_result.error().message(), Catch::Matchers::ContainsSubstring("Key"));
    });

    RawClient client(fix.port);
    client.send_raw_http("GET / HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Version: 13\r\n"
                         "\r\n");
    client.recv_some();

    server.get();
}

TEST_CASE("Non-localhost Origin produces a WSWarning, not an error", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto warning) {
        // Connection should succeed despite the foreign origin.
        REQUIRE(ws_result.has_value());
        // But a warning should be recorded.
        CHECK_FALSE(warning.empty());
        CHECK_THAT(warning.message(), Catch::Matchers::ContainsSubstring("http://evil.example.com"));
    });

    RawClient client(fix.port);
    client.do_handshake("http://evil.example.com");
    // Consume the greeting-less stream - send a close so the server tears down.
    client.send_frame(0x08, "\x03\xe8"); // close, code 1000

    server.get();
}

TEST_CASE("Missing Origin header produces a WSWarning", "[handshake]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto warning) {
        REQUIRE(ws_result.has_value());
        CHECK_FALSE(warning.empty());
        CHECK_THAT(warning.message(), Catch::Matchers::ContainsSubstring("missing Origin"));
    });

    RawClient client(fix.port);
    client.do_handshake(""); // empty -> no Origin header
    client.send_frame(0x08, "\x03\xe8");

    server.get();
}

TEST_CASE("Localhost origins (127.0.0.1 and [::1]) do not trigger a warning", "[handshake]")
{
    for (const auto& origin : {
             std::string("http://localhost"),
             std::string("http://127.0.0.1"),
             std::string("http://[::1]"),
             std::string("http://localhost:3000"),
             std::string("https://127.0.0.1:8080"),
         })
    {
        ServerFixture fix;
        auto server = fix.run_server([](auto ws_result, auto warning) {
            REQUIRE(ws_result.has_value());
            CHECK(warning.empty());
            // Close cleanly.
            (void)(*ws_result)->send_close();
        });

        RawClient client(fix.port);
        client.do_handshake(origin);
        client.recv_close_frame();

        server.get();
    }
}

// ===========================================================================
// Section 3: Sending and receiving messages
// ===========================================================================

TEST_CASE("send_text / receive_message round-trip", "[messaging]")
{
    ServerFixture fix;

    const std::string echo_msg = "Hello, WebSocket!";

    auto server = fix.run_server([&](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        auto& ws = *ws_result;

        std::string payload;
        auto op = ws->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(*op == kb::net::WSOpcode::text);
        CHECK(payload == echo_msg);

        // Echo it back.
        CHECK(ws->send_text(payload).has_value());
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x01, echo_msg); // opcode 0x01 = text
    const auto reply = client.recv_text_frame();
    CHECK(reply == echo_msg);

    server.get();
}

TEST_CASE("Binary message is received with WSOpcode::binary", "[messaging]")
{
    ServerFixture fix;

    const std::string data = "\x00\x01\x02\x03\xFF\xFE";

    auto server = fix.run_server([&](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        auto& ws = *ws_result;

        std::string payload;
        auto op = ws->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(*op == kb::net::WSOpcode::binary);
        CHECK(payload == data);
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x02, data); // opcode 0x02 = binary

    server.get();
}

TEST_CASE("Empty text message is received correctly", "[messaging]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(*op == kb::net::WSOpcode::text);
        CHECK(payload.empty());
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x01, "");

    server.get();
}

TEST_CASE("Large message (>125 bytes, 16-bit length field) round-trip", "[messaging]")
{
    ServerFixture fix;

    // 300-byte payload - exercises the 16-bit extended-length path.
    const std::string big(300, 'X');

    auto server = fix.run_server([&](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(payload == big);
        CHECK((*ws_result)->send_text(payload).has_value());
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x01, big);
    const auto reply = client.recv_text_frame();
    CHECK(reply == big);

    server.get();
}

TEST_CASE("Multiple sequential messages are received in order", "[messaging]")
{
    ServerFixture fix;

    const std::vector<std::string> msgs = {"first", "second", "third"};

    auto server = fix.run_server([&](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        auto& ws = *ws_result;
        for (const auto& expected : msgs)
        {
            std::string payload;
            auto op = ws->receive_message(payload);
            REQUIRE(op.has_value());
            CHECK(payload == expected);
        }
    });

    RawClient client(fix.port);
    client.do_handshake();
    for (const auto& m : msgs)
    {
        client.send_frame(0x01, m);
    }

    server.get();
}

// ===========================================================================
// Section 4: Fragmented messages
// ===========================================================================

TEST_CASE("Two-fragment text message is reassembled", "[fragmentation]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(*op == kb::net::WSOpcode::text);
        CHECK(payload == "HelloWorld");
    });

    RawClient client(fix.port);
    client.do_handshake();
    // Fragment 1: FIN=0, opcode=text (0x01)
    client.send_frame(0x01, "Hello", /*fin=*/false);
    // Fragment 2: FIN=1, opcode=continuation (0x00)
    client.send_frame(0x00, "World", /*fin=*/true);

    server.get();
}

TEST_CASE("Three-fragment message is reassembled", "[fragmentation]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(payload == "ABCDEFGHI");
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x01, "ABC", false);
    client.send_frame(0x00, "DEF", false);
    client.send_frame(0x00, "GHI", true);

    server.get();
}

TEST_CASE("Ping interleaved with fragmented data is handled correctly", "[fragmentation]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        // receive_message should silently handle the interleaved ping and pong.
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(payload == "PartOnePartTwo");
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x01, "PartOne", false); // first fragment
    client.send_frame(0x09, "ping payload");   // ping (control, FIN=1)
    client.send_frame(0x00, "PartTwo", true);  // last fragment
    // Consume the pong the server sends back.
    // The pong frame: opcode 0x0A, FIN set, unmasked, payload "ping payload"
    // We can just drain a few bytes - exact pong reading is not critical here.
    client.recv_some(64);

    server.get();
}

// ===========================================================================
// Section 5: Control frames
// ===========================================================================

TEST_CASE("Ping frame triggers an automatic Pong", "[control]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        // Block in receive_message - the ping/pong will be handled internally.
        // We send a text frame after so receive_message can return.
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(payload == "after ping");
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x09, "echo me"); // ping
    // Read the pong (opcode 0x0A)
    char pong_header[2];
    client.recv_exact(pong_header, 2);
    CHECK((static_cast<uint8_t>(pong_header[0]) & 0x0Fu) == 0x0Au); // pong opcode
    const uint8_t pong_len = static_cast<uint8_t>(pong_header[1]) & 0x7Fu;
    std::string pong_body(pong_len, '\0');
    if (pong_len > 0)
    {
        client.recv_exact(pong_body.data(), pong_len);
    }
    CHECK(pong_body == "echo me");

    // Now send a regular text frame so receive_message returns.
    client.send_frame(0x01, "after ping");

    server.get();
}

TEST_CASE("Close frame is echoed back and receive_message returns WSOpcode::close", "[control]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE(op.has_value());
        CHECK(*op == kb::net::WSOpcode::close);
    });

    RawClient client(fix.port);
    client.do_handshake();
    // Send close with code 1000 (normal closure), masked.
    client.send_frame(0x08, "\x03\xe8");
    // The server echoes a close frame back.
    const uint16_t code = client.recv_close_frame();
    CHECK(code == 1000);

    server.get();
}

TEST_CASE("send_close is idempotent", "[control]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        auto& ws = *ws_result;
        // First close should send a frame.
        CHECK(ws->send_close(1000).has_value());
        // Second call should be a no-op (returns success without sending).
        CHECK(ws->send_close(1000).has_value());
    });

    RawClient client(fix.port);
    client.do_handshake();
    // Receive exactly one close frame.
    const uint16_t code = client.recv_close_frame();
    CHECK(code == 1000);
    // The socket should receive no further data (or peer closes).

    server.get();
}

// ===========================================================================
// Section 6: Protocol-error rejection
// ===========================================================================

TEST_CASE("RSV bits set on a data frame triggers a WSError", "[protocol]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE_FALSE(op.has_value());
        CHECK_THAT(op.error().message(), Catch::Matchers::ContainsSubstring("RSV"));
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_rsv_frame(0x01, 0x40, "bad frame"); // RSV2 set

    server.get();
}

TEST_CASE("Non-minimal 16-bit payload length encoding is rejected", "[protocol]")
{
    // A payload of 100 bytes encoded with the 16-bit (126) indicator instead of
    // the 7-bit direct form is a protocol violation (RFC 6455 §5.2).
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE_FALSE(op.has_value());
        CHECK_THAT(op.error().message(), Catch::Matchers::ContainsSubstring("non-minimal"));
    });

    RawClient client(fix.port);
    client.do_handshake();

    // Manually craft a frame with len=100 encoded as 16-bit.
    const std::string body(100, 'A');
    std::vector<char> frame;
    frame.push_back(static_cast<char>(0x81u));        // FIN + text
    frame.push_back(static_cast<char>(0x80u | 126u)); // mask bit + 16-bit indicator
    frame.push_back(0x00);
    frame.push_back(static_cast<char>(100u)); // 16-bit big-endian length = 100
    const std::array<uint8_t, 4> mask = {0xAA, 0xBB, 0xCC, 0xDD};
    for (auto b : mask)
    {
        frame.push_back(static_cast<char>(b));
    }
    for (size_t i = 0; i < body.size(); ++i)
    {
        frame.push_back(static_cast<char>(static_cast<uint8_t>(body[i]) ^ mask[i % 4]));
    }

    client.send_raw(frame.data(), frame.size());

    server.get();
}

TEST_CASE("Fragmented control frame is rejected", "[protocol]")
{
    // Control frames (ping/pong/close) must not be fragmented (RFC 6455 §5.5).
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE_FALSE(op.has_value());
        CHECK_THAT(op.error().message(), Catch::Matchers::ContainsSubstring("fragmented control"));
    });

    RawClient client(fix.port);
    client.do_handshake();
    // Ping with FIN=0 - invalid.
    client.send_frame(0x09, "frag ping", /*fin=*/false);

    server.get();
}

TEST_CASE("Control frame with payload > 125 bytes is rejected", "[protocol]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        std::string payload;
        auto op = (*ws_result)->receive_message(payload);
        REQUIRE_FALSE(op.has_value());
        CHECK_THAT(op.error().message(), Catch::Matchers::ContainsSubstring("125"));
    });

    RawClient client(fix.port);
    client.do_handshake();

    // Craft a ping with 126-byte payload - must use the 16-bit length form.
    const std::string big_ping(126, 'P');
    std::vector<char> frame;
    frame.push_back(static_cast<char>(0x89u));        // FIN + ping (0x09)
    frame.push_back(static_cast<char>(0x80u | 126u)); // mask + 16-bit indicator
    frame.push_back(0x00);
    frame.push_back(static_cast<char>(126u));
    const std::array<uint8_t, 4> mask = {0x01, 0x02, 0x03, 0x04};
    for (auto b : mask)
    {
        frame.push_back(static_cast<char>(b));
    }
    for (size_t i = 0; i < big_ping.size(); ++i)
    {
        frame.push_back(static_cast<char>(static_cast<uint8_t>(big_ping[i]) ^ mask[i % 4]));
    }

    client.send_raw(frame.data(), frame.size());

    server.get();
}

// ===========================================================================
// Section 7: Peer metadata
// ===========================================================================

TEST_CASE("WebSocketStream exposes peer IP and port", "[metadata]")
{
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        auto& ws = *ws_result;
        CHECK_FALSE(ws->get_peer_ip().empty());
        CHECK(ws->get_peer_port() != 0);
        // The peer is 127.0.0.1 (we connected from loopback).
        CHECK(ws->get_peer_ip() == "127.0.0.1");
    });

    RawClient client(fix.port);
    client.do_handshake();
    client.send_frame(0x08, "\x03\xe8"); // close
    client.recv_close_frame();

    server.get();
}

// ===========================================================================
// Section 8: Destructor / RAII teardown
// ===========================================================================

TEST_CASE("WebSocketStream destructor sends a close frame", "[teardown]")
{
    // When the WebSocketStream is destroyed the dtor calls send_close().
    // The raw client should receive a close frame before the TCP connection drops.
    ServerFixture fix;

    auto server = fix.run_server([](auto ws_result, auto /*warning*/) {
        REQUIRE(ws_result.has_value());
        // Let ws_result go out of scope immediately - destructor fires.
    });

    RawClient client(fix.port);
    client.do_handshake();

    // We should receive a close frame (code 1000) from the server's dtor.
    const uint16_t code = client.recv_close_frame();
    CHECK(code == 1000);

    server.get();
}