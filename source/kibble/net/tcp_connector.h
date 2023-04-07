#pragma once

#include <string>

namespace kb
{
namespace net
{

class TCPStream;

/**
 * @brief Stateless static class that can connect to a remote TCPAcceptor.
 * The connect() function (non-blockingly) attempts to connect to the remote TCPAcceptor and returns a stream that can
 * be used for bidirectional communication with the TCPAcceptor.
 *
 * @note At the moment, only a linux implementation is available.
 */
class TCPConnector
{
public:
    /**
     * @brief Generate a TCP stream object by connecting to a server using its name or IP address, and a port.
     *
     * @param server server's IP address
     * @param port the same port the server is listening to
     * @return new stream pointer. Caller is responsible for its destruction.
     */
    static TCPStream* connect(const std::string& server, uint16_t port);
};

} // namespace net
} // namespace kb