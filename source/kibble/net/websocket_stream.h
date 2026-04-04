#pragma once

#include "kibble/net/error.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace kb::net
{

class TCPStream;

/**
 * @brief WebSocket opcodes as defined in RFC 6455 §11.8.
 */
enum class WSOpcode : uint8_t
{
    // clang-format off
    continuation = 0x00u,
    text         = 0x01u,
    binary       = 0x02u,
    close        = 0x08u,
    ping         = 0x09u,
    pong         = 0x0Au,
    // clang-format on
};

/**
 * @brief Represents an active WebSocket connection.
 *
 * Wraps a TCPStream and adds the RFC 6455 framing protocol on top.
 * Constructed exclusively by WebSocketAcceptor after a successful HTTP upgrade handshake.
 *
 * Fragmented messages (FIN=0) are reassembled transparently inside receive_message().
 * Control frames (ping/pong/close) are handled automatically during reassembly.
 *
 * All I/O operations return `std::expected<T, WSError>`. A `WSError` carries either
 * an OS-level `NetError` or a WebSocket protocol violation string, letting callers
 * decide how to log or handle failures.
 *
 * The close handshake is initiated automatically in the destructor on a best-effort
 * basis (the result is intentionally ignored, as in standard RAII teardown).
 *
 * @note Non-copyable. Takes ownership of the TCPStream pointer passed to it.
 */
class WebSocketStream
{
public:
    friend class WebSocketAcceptor;

    /// @brief Send a best-effort close frame, then destroy the underlying TCP stream.
    ~WebSocketStream();

    // Non-copyable, non-movable (holds an owning unique_ptr and a closed_ flag).
    WebSocketStream(const WebSocketStream&) = delete;
    WebSocketStream& operator=(const WebSocketStream&) = delete;

    /// @brief Remote port of the underlying TCP connection.
    [[nodiscard]] uint16_t get_peer_port() const;

    /// @brief Remote IP address of the underlying TCP connection.
    [[nodiscard]] const std::string& get_peer_ip() const;

    /**
     * @brief Send a text message to the peer.
     *
     * @param msg UTF-8 string to send
     * @return void on success, or a WSError on write failure
     */
    [[nodiscard]] std::expected<void, WSError> send_text(const std::string& msg);

    /**
     * @brief Send a binary message to the peer.
     *
     * @param data pointer to the payload bytes
     * @param len  number of bytes to send
     * @return void on success, or a WSError on write failure
     */
    [[nodiscard]] std::expected<void, WSError> send_binary(const char* data, size_t len);

    /**
     * @brief Receive one complete message from the peer.
     *
     * Reassembles fragmented frames and handles control frames automatically
     * (pings are answered with pongs; close frames trigger send_close()).
     *
     * On success the returned `WSOpcode` is either `text` or `binary`.
     * A `close` opcode is returned (not an error) when the peer closes cleanly.
     *
     * @param payload output: reassembled message payload (cleared on entry)
     * @return the opcode of the received message (text / binary / close),
     *         or a WSError on unrecoverable read or protocol error
     */
    [[nodiscard]] std::expected<WSOpcode, WSError> receive_message(std::string& payload);

    /**
     * @brief Send a close frame to the peer.
     *
     * Idempotent - subsequent calls are silently ignored.
     *
     * @param code RFC 6455 status code (default 1000 = normal closure)
     * @return void on success, or a WSError if the underlying send fails
     */
    [[nodiscard]] std::expected<void, WSError> send_close(uint16_t code = 1000);

private:
    explicit WebSocketStream(std::unique_ptr<TCPStream> stream);

    /**
     * @brief Send a raw WebSocket frame (server-side, unmasked).
     *
     * @param opcode  frame opcode
     * @param payload pointer to payload bytes (may be nullptr if len is 0)
     * @param len     payload length in bytes
     * @param fin     whether to set the FIN bit (default true)
     */
    [[nodiscard]] std::expected<void, WSError> send_frame(WSOpcode opcode, const char* payload, size_t len,
                                                          bool fin = true);

    /**
     * @brief Send a pong frame in response to a ping.
     *
     * @param payload the ping payload to echo back (RFC 6455 §5.5.3)
     */
    [[nodiscard]] std::expected<void, WSError> send_pong(const std::string& payload);

    /**
     * @brief Read one raw frame from the peer.
     *
     * Validates framing rules (RSV bits, minimal length encoding, control-frame
     * constraints) and unmasks client-to-server frames in place.
     *
     * @param opcode  output: frame opcode
     * @param payload output: unmasked frame payload
     * @param fin     output: FIN bit
     */
    [[nodiscard]] std::expected<void, WSError> read_frame(WSOpcode& opcode, std::string& payload, bool& fin);

private:
    std::unique_ptr<TCPStream> stream_;
    bool closed_{false};

    /// @brief Reusable scratch buffer for reading individual frames inside
    ///        receive_message(). Kept as a member to preserve allocated capacity
    ///        across calls and avoid repeated heap allocations for typical message
    ///        sizes.
    std::string frame_payload_;
};

} // namespace kb::net