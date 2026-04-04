#include "kibble/net/websocket_acceptor.h"
#include "kibble/hash/sha1.h"
#include "kibble/net/tcp_stream.h"
#include "kibble/net/websocket_stream.h"
#include "kibble/string/base64.h"
#include "kibble/util/unordered_dense.h"
#include "kibble/string/string.h"

#include <string>
#include <string_view>

namespace kb::net
{

namespace
{

// ---- Constants ----------------------------------------------------------

// RFC 6455 §1.3 - concatenated with the client key before hashing.
constexpr const char* k_ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

constexpr const char* k_http_switching_protocols = "HTTP/1.1 101 Switching Protocols\r\n"
                                                   "Upgrade: websocket\r\n"
                                                   "Connection: Upgrade\r\n";

constexpr const char* k_http_bad_request = "HTTP/1.1 400 Bad Request\r\n"
                                           "Connection: close\r\n"
                                           "\r\n";

// ---- Helpers --------------------------------------------------

/**
 * @internal
 * @brief Read a complete HTTP request from the stream.
 *
 * Reads in chunks of up to `k_read_chunk` bytes per syscall, accumulating
 * into a `std::string` until the `\r\n\r\n` header terminator is found.
 *
 * To avoid missing a terminator that straddles two chunks, we track the last
 * three bytes of the previously received data and search the overlap region
 * (those 3 bytes + the new chunk) rather than rescanning the entire buffer.
 * The search window is therefore at most `k_read_chunk + 3` bytes per
 * iteration regardless of total request size.
 *
 * A hard cap of `k_max_request_bytes` guards against unbounded memory growth
 * from a misbehaving or malicious client that never sends `\r\n\r\n`.
 *
 * @return the raw HTTP request string (including the terminal `\r\n\r\n`),
 *         or a WSError on TCP failure or if the request exceeds the size cap.
 */
std::expected<std::string, WSError> read_http_request(TCPStream& stream)
{
    // 1 KiB chunks: large enough to receive a typical HTTP upgrade request in
    // one or two syscalls, small enough to live comfortably on the stack.
    static constexpr size_t k_read_chunk = 1024;
    // 16 KiB matches common HTTP server limits for header sections.
    static constexpr size_t k_max_request_bytes = 16 * 1024;

    std::string request;
    request.reserve(k_read_chunk);

    // How many tail bytes of `request` to include in the terminator search on
    // the next iteration. We need up to 3 bytes of overlap so that a
    // \r\n\r\n split across a chunk boundary is never missed.
    static constexpr size_t k_overlap = 3;
    // Temporary chunk buffer; lives on the stack for the duration of the call.
    char buf[k_read_chunk];

    while (true)
    {
        // receive_exact blocks until exactly k_read_chunk bytes arrive, which
        // would stall if the client sends a short request. We therefore use
        // receive (non-exact), which returns however many bytes are currently
        // available (at least 1). The returned ssize_t is the byte count.
        const auto r = stream.receive(buf, k_read_chunk);
        if (!r)
        {
            return std::unexpected(WSError::tcp(r.error()));
        }
        const size_t n = static_cast<size_t>(*r);

        // Guard against oversized / malformed requests.
        if (request.size() + n > k_max_request_bytes)
        {
            return std::unexpected(WSError::protocol("HTTP request exceeds maximum allowed size"));
        }

        // Search window: up to k_overlap bytes already in `request` plus
        // the new chunk, so a \r\n\r\n straddling the boundary is caught.
        const size_t search_start = request.size() > k_overlap ? request.size() - k_overlap : 0;

        request.append(buf, n);

        // Find \r\n\r\n in the window [search_start, end).
        const size_t found = request.find("\r\n\r\n", search_start);
        if (found != std::string::npos)
        {
            // Trim to exactly the end of the header section.
            request.resize(found + 4);
            break;
        }
    }

    return request;
}

/**
 * @internal
 * @brief Parse HTTP request headers into a lowercase-keyed map.
 *
 * Operates directly on a `std::string_view` of the raw request using a
 * lightweight state machine.
 *
 * States
 * ------
 *  1. Skip the request-line (everything up to and including the first `\n`).
 *  2. For each subsequent line:
 *     a. Collect characters up to `:` -> that is the header name; lowercase in place.
 *     b. Skip the `:` and any leading SP/HT whitespace.
 *     c. Collect characters up to `\r\n` (or bare `\n`) -> that is the header value.
 *     d. An empty line signals the end of the header section.
 *
 * The request-line skip and empty-line detection handle both `\r\n` and bare
 * `\n` line endings for robustness, though RFC 7230 mandates `\r\n`.
 */
ankerl::unordered_dense::map<std::string, std::string> parse_headers(std::string_view raw)
{
    ankerl::unordered_dense::map<std::string, std::string> headers;

    // --- State 1: skip the request-line ---
    const size_t first_lf = raw.find('\n');
    if (first_lf == std::string_view::npos)
    {
        return headers; // malformed: no lines at all
    }
    size_t pos = first_lf + 1;

    // --- State 2: parse header lines ---
    while (pos < raw.size())
    {
        // Detect end-of-headers (bare \r\n or \n).
        if (raw[pos] == '\r' || raw[pos] == '\n')
        {
            break;
        }

        // --- 2a: collect and lowercase the header name up to ':' ---
        const size_t name_start = pos;
        while (pos < raw.size() && raw[pos] != ':' && raw[pos] != '\r' && raw[pos] != '\n')
        {
            ++pos;
        }
        if (pos >= raw.size() || raw[pos] != ':')
        {
            // Malformed line (no colon); skip to next line.
            while (pos < raw.size() && raw[pos] != '\n')
            {
                ++pos;
            }
            if (pos < raw.size())
            {
                ++pos; // consume '\n'
            }
            continue;
        }

        std::string name(raw.substr(name_start, pos - name_start));
        su::to_lower(name);
        ++pos; // skip ':'

        // --- 2b: skip leading SP / HT ---
        while (pos < raw.size() && (raw[pos] == ' ' || raw[pos] == '\t'))
        {
            ++pos;
        }

        // --- 2c: collect the value up to CR or LF ---
        const size_t value_start = pos;
        while (pos < raw.size() && raw[pos] != '\r' && raw[pos] != '\n')
        {
            ++pos;
        }
        std::string value(raw.substr(value_start, pos - value_start));

        // --- advance past the line ending (\r\n or \n) ---
        if (pos < raw.size() && raw[pos] == '\r')
        {
            ++pos;
        }
        if (pos < raw.size() && raw[pos] == '\n')
        {
            ++pos;
        }

        headers.emplace(std::move(name), std::move(value));
    }

    return headers;
}

/**
 * @internal
 * @brief Compute the Sec-WebSocket-Accept value (RFC 6455 §4.2.2).
 *
 *   accept = base64( SHA1_raw( client_key + k_ws_guid ) )
 *
 * SHA1::final_bytes() returns the 20 raw digest bytes directly, avoiding the
 * hex-encode / hex-decode roundtrip that would be needed with final().
 */
std::string compute_accept_key(const std::string& client_key)
{
    const std::string combined = client_key + k_ws_guid;
    kb::hash::SHA1 sha;
    sha.update(combined);
    const auto raw = sha.final_bytes(); // std::array<uint8_t, 20>
    return su::base64_encode(raw);
}

/**
 * @internal
 * @brief Validate mandatory WebSocket upgrade headers and check Origin.
 *
 * Hard failures (missing or wrong Upgrade / Connection / Version) are returned
 * as `std::unexpected<WSError>`.
 *
 * Soft failures that the server has chosen not to reject outright - specifically
 * a missing Origin header or a non-localhost Origin - are written to `*warning`
 * when the pointer is non-null, letting the caller decide whether to log or
 * escalate. `*warning` is left untouched when no soft failure occurs, so callers
 * can initialise it once and pass it through multiple calls.
 *
 * @param headers lowercase-keyed header map
 * @param warning optional out-param for non-fatal diagnostics (may be nullptr)
 * @return void on success, or a WSError::protocol describing the hard failure
 */
std::expected<void, WSError> validate_upgrade_headers(
    const ankerl::unordered_dense::map<std::string, std::string>& headers, WSWarning* warning)
{
    // Upgrade: websocket
    auto it = headers.find("upgrade");
    if (it == headers.end())
    {
        return std::unexpected(WSError::protocol("missing Upgrade header"));
    }
    std::string upgrade_val = it->second;
    for (char& ch : upgrade_val)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (upgrade_val != "websocket")
    {
        return std::unexpected(WSError::protocol("Upgrade header is not 'websocket'"));
    }

    // Connection: Upgrade (substring match, case-insensitive)
    it = headers.find("connection");
    if (it == headers.end())
    {
        return std::unexpected(WSError::protocol("missing Connection header"));
    }
    std::string connection_val = it->second;
    for (char& ch : connection_val)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (connection_val.find("upgrade") == std::string::npos)
    {
        return std::unexpected(WSError::protocol("Connection header does not contain 'Upgrade'"));
    }

    // Sec-WebSocket-Version: 13
    it = headers.find("sec-websocket-version");
    if (it == headers.end() || it->second != "13")
    {
        return std::unexpected(WSError::protocol("Sec-WebSocket-Version must be 13"));
    }

    // Origin validation (RFC 6455 §10.2 / RFC 6454): WebSocket is not restricted
    // by the browser same-origin policy, so the server must check it explicitly.
    // We warn rather than reject for non-localhost origins (dev-server policy).
    //
    // A well-formed Origin header is a serialised origin of the form:
    //
    //   scheme "://" host [ ":" port ]
    //
    // where host is one of:
    //   - "localhost"
    //   - "127.0.0.1"
    //   - "[::1]"       (IPv6 loopback, bracket-quoted per RFC 3986)
    //
    // We parse the host component directly from the value rather than doing a
    // substring search, which would incorrectly pass crafted values such as
    // "http://evil.localhost.example.com" or "http://notlocalhost".
    //
    // The check is intentionally scheme-agnostic (both http:// and https:// are
    // accepted) because browsers may use either for loopback origins depending
    // on context (e.g. secure contexts over a local TLS proxy).
    it = headers.find("origin");
    if (it == headers.end())
    {
        if (warning != nullptr)
        {
            warning->text = "missing Origin header";
        }
    }
    else
    {
        // Lowercase the entire value once for case-insensitive comparisons.
        std::string_view raw_origin = it->second;

        // Find the "://" authority separator.
        const size_t scheme_end = it->second.find("://");
        const bool is_localhost = [&]() -> bool
        {
            if (scheme_end == std::string::npos)
            {
                return false; // not a valid absolute origin
            }

            // The host starts immediately after "://".
            std::string_view host_and_port = raw_origin.substr(scheme_end + 3);

            // Strip an optional port suffix ":NNN" at the end.
            // IPv6 addresses are enclosed in brackets "[...]" per RFC 3986 §3.2.2;
            // their closing ']' must appear before any port colon.
            std::string_view host = host_and_port;
            if (!host_and_port.empty() && host_and_port.front() == '[')
            {
                // IPv6 bracketed host: find the closing ']'.
                const size_t bracket_close = host_and_port.find(']');
                if (bracket_close == std::string_view::npos)
                {
                    return false; // malformed IPv6 literal
                }
                // host is everything up to and including ']'.
                host = host_and_port.substr(0, bracket_close + 1);
            }
            else
            {
                // Non-bracketed host: strip from the first ':' onward (port).
                const size_t colon = host_and_port.find(':');
                if (colon != std::string_view::npos)
                {
                    host = host_and_port.substr(0, colon);
                }
            }

            // Case-insensitive comparison for the host token.
            // Hostnames are ASCII; manual tolower avoids locale dependency.
            auto ci_equal = [](std::string_view a, std::string_view b) -> bool
            {
                if (a.size() != b.size())
                {
                    return false;
                }
                for (size_t ii = 0; ii < a.size(); ++ii)
                {
                    if (std::tolower(static_cast<unsigned char>(a[ii])) !=
                        std::tolower(static_cast<unsigned char>(b[ii])))
                    {
                        return false;
                    }
                }
                return true;
            };

            return ci_equal(host, "localhost") || ci_equal(host, "127.0.0.1") || ci_equal(host, "[::1]");
        }();

        if (!is_localhost && warning != nullptr)
        {
            warning->text = "unexpected Origin: " + it->second;
        }
    }

    return {};
}

} // anonymous namespace

// ---- WebSocketAcceptor --------------------------------------------------

WebSocketAcceptor::WebSocketAcceptor(uint16_t port, const char* address) : acceptor_(port, address)
{
}

std::expected<void, WSError> WebSocketAcceptor::start()
{
    if (auto r = acceptor_.start(); !r)
    {
        return std::unexpected(WSError::tcp(r.error()));
    }
    return {};
}

std::expected<std::unique_ptr<WebSocketStream>, WSError> WebSocketAcceptor::accept(WSWarning* warning)
{
    // Clear any stale warning from a previous call.
    if (warning != nullptr)
    {
        warning->text.clear();
    }

    // --- Step 1: TCP accept ---
    auto tcp_result = acceptor_.accept();
    if (!tcp_result)
    {
        return std::unexpected(WSError::tcp(tcp_result.error()));
    }
    std::unique_ptr<TCPStream> tcp = std::move(*tcp_result);

    // --- Step 2: read HTTP upgrade request ---
    auto request_result = read_http_request(*tcp);
    if (!request_result)
    {
        // tcp is destroyed here; the WSError carries the reason.
        return std::unexpected(request_result.error());
    }

    // --- Step 3: parse and validate headers ---
    const auto headers = parse_headers(*request_result);

    if (auto r = validate_upgrade_headers(headers, warning); !r)
    {
        // Best-effort rejection response - ignore send errors at this point.
        (void)tcp->send(k_http_bad_request); // NOLINT(bugprone-unused-return-value)
        return std::unexpected(r.error());
    }

    const auto key_it = headers.find("sec-websocket-key");
    if (key_it == headers.end())
    {
        (void)tcp->send(k_http_bad_request); // NOLINT(bugprone-unused-return-value)
        return std::unexpected(WSError::protocol("missing Sec-WebSocket-Key header"));
    }

    // --- Step 4: build and send the 101 Switching Protocols response ---
    const std::string accept_key = compute_accept_key(key_it->second);

    std::string response = k_http_switching_protocols;
    response += "Sec-WebSocket-Accept: ";
    response += accept_key;
    response += "\r\n\r\n";

    if (auto r = tcp->send(response); !r)
    {
        return std::unexpected(WSError::tcp(r.error()));
    }

    // --- Step 5: hand ownership to a WebSocketStream ---
    // WebSocketStream's constructor is private; WebSocketAcceptor is a friend.
    return std::unique_ptr<WebSocketStream>(new WebSocketStream(std::move(tcp)));
}

} // namespace kb::net