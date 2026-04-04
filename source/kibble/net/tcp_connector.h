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
 * @brief Stateless helper that establishes an outgoing TCP connection.
 *
 * connect() performs DNS resolution (falling back to a literal IP string),
 * creates the socket, and connects to the remote acceptor - all non-blockingly
 * from the caller's perspective (the underlying `connect()` syscall does block
 * until the TCP handshake completes or times out).
 *
 * On success a `unique_ptr<TCPStream>` is returned; ownership is transferred
 * to the caller. On failure a `NetError` is returned instead.
 *
 * @note Linux and Windows implementations are available.
 */
class TCPConnector
{
public:
    /**
     * @brief Connect to a remote TCPAcceptor.
     *
     * @param server hostname or IPv4 address string
     * @param port   the port the server is listening on
     * @return a connected stream, or a NetError on failure
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TCPStream>, NetError> connect(const std::string& server,
                                                                                     uint16_t port);
};

} // namespace kb::net