#pragma once

#include "kibble/net/common.h"
#include "kibble/net/error.h"

#include <cstdint>
#include <expected>
#include <string>

namespace kb::net
{

/**
 * @brief Used for bidirectional communication between a client and a server.
 * It represents an active connection, created either actively by a TCPConnector,
 * or passively by a TCPAcceptor.
 *
 * All I/O operations return `std::expected<T, NetError>` so the caller can
 * decide how to handle (log, rethrow, ignore) failures without coupling the
 * library to any particular logging back-end.
 *
 * @note This object is non-copyable and its constructor is private.
 *       Only TCPAcceptor and TCPConnector can create it.
 * @note Linux and Windows implementations are available.
 */
class TCPStream
{
public:
    friend class TCPAcceptor;
    friend class TCPConnector;

    /**
     * @brief Close the socket and destroy the stream.
     */
    ~TCPStream();

    // Non-copyable, movable.
    TCPStream(const TCPStream&) = delete;
    TCPStream& operator=(const TCPStream&) = delete;
    TCPStream(TCPStream&&) = default;
    TCPStream& operator=(TCPStream&&) = default;

    /// Remote port of this connection.
    [[nodiscard]] uint16_t get_peer_port() const
    {
        return peer_port_;
    }

    /// Remote IP address of this connection.
    [[nodiscard]] const std::string& get_peer_ip() const
    {
        return peer_ip_;
    }

    /**
     * @brief Send a data buffer.
     *
     * @param buffer pointer to the data buffer
     * @param len    number of bytes to transmit
     * @return number of bytes written, or a NetError on failure
     */
    [[nodiscard]] std::expected<detail::ssize_t, NetError> send(const char* buffer, size_t len);

    /**
     * @brief Convenience overload - send a string.
     */
    [[nodiscard]] std::expected<detail::ssize_t, NetError> send(const std::string& msg)
    {
        return send(msg.c_str(), msg.size());
    }

    /**
     * @brief Non-exact read: returns as soon as any data is available,
     *        which may be fewer bytes than `len`.
     *
     * @param buffer destination buffer (at least `len` bytes)
     * @param len    maximum number of bytes to read
     * @return number of bytes read (0 = connection closed), or a NetError
     */
    [[nodiscard]] std::expected<detail::ssize_t, NetError> receive(char* buffer, size_t len);

    /**
     * @brief Exact read: blocks until exactly `len` bytes have arrived.
     *
     * @param buffer destination buffer (at least `len` bytes)
     * @param len    exact number of bytes to read
     * @return `true` on success, or a NetError on connection close / IO error
     */
    [[nodiscard]] std::expected<void, NetError> receive_exact(char* buffer, size_t len);

    /**
     * @brief Read available data and append it to `msg`.
     *        Loops until fewer than the internal buffer size bytes arrive
     *        (i.e. no more data is immediately pending).
     *
     * @param msg target string - data is *appended*
     * @return `void` on success, or a NetError on failure
     */
    [[nodiscard]] std::expected<void, NetError> receive(std::string& msg);

private:
    TCPStream() = default;
    TCPStream(detail::socket_t fd, void* address_in);

private:
    detail::socket_t fd_ = detail::k_invalid_socket;
    uint16_t peer_port_{};
    std::string peer_ip_;
};

} // namespace kb::net