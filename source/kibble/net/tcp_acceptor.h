#pragma once

#include "kibble/net/common.h"
#include "kibble/net/error.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace kb::net
{

class TCPStream;

/**
 * @brief Berkeley/Winsock socket wrapper that accepts incoming TCP connections.
 *
 * Call start() once to bind and begin listening, then call accept() in a loop
 * to receive connections. Each successful accept() yields a heap-allocated
 * TCPStream wrapped in a `unique_ptr`; ownership is transferred to the caller.
 *
 * Failures are reported as `std::expected<…, NetError>` so the caller can
 * decide how to handle or log them.
 *
 * @note Linux and Windows implementations are available.
 */
class TCPAcceptor
{
public:
    /**
     * @brief Construct a TCPAcceptor.
     *        On Windows, initialises Winsock (WSAStartup) on first use.
     *
     * @param port    TCP port to listen on
     * @param address Interface address to bind to. Empty string -> INADDR_ANY
     *                (accept connections on all interfaces).
     */
    explicit TCPAcceptor(uint16_t port, const char* address = "");

    /// Close the listening socket.
    ~TCPAcceptor();

    // Non-copyable, movable.
    TCPAcceptor(const TCPAcceptor&) = delete;
    TCPAcceptor& operator=(const TCPAcceptor&) = delete;
    TCPAcceptor(TCPAcceptor&&) = default;
    TCPAcceptor& operator=(TCPAcceptor&&) = default;

    /**
     * @brief Bind the socket and start listening.
     *        Idempotent: calling start() on an already-listening acceptor is a no-op.
     *
     * @return `void` on success, or a NetError describing what went wrong.
     */
    [[nodiscard]] std::expected<void, NetError> start();

    /**
     * @brief Blockingly accept the next incoming connection.
     *
     * @return A `unique_ptr<TCPStream>` for the new connection,
     *         or a NetError if the acceptor is not listening / accept() fails.
     */
    [[nodiscard]] std::expected<std::unique_ptr<TCPStream>, NetError> accept();

private:
    detail::socket_t lfd_;
    uint16_t port_;
    bool listening_;
    std::string address_;
};

} // namespace kb::net