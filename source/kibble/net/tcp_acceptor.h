#pragma once
#include <string>

namespace kb
{
namespace net
{

class TCPStream;

/**
 * @brief Berkeley socket wrapper that can accept TCP connections.
 * Once the TCPAcceptor has started listening, the accept() function is used to (blockingly) accept the first incoming
 * connection from a remote TCPConnector. A TCPStream pointer is returned which can be used for bidirectional
 * communication with the TCPConnector.
 * The user of this code takes full ownership of the stream pointer, and is responsible for deleting it.
 *
 */
class TCPAcceptor
{
public:
    /**
     * @brief Construct a new TCPAcceptor
     *
     * @param port TCP port to listen to
     * @param address address to bind the socket to. If left empty, the socket will be bound to all available
     * interfaces, meaning it will accept connections from any address.
     */
    TCPAcceptor(uint16_t port, const char *address = "");

    /**
     * @brief Close internal file descriptor and destroy acceptor
     *
     */
    ~TCPAcceptor();

    /**
     * @brief Start listening on port
     *
     * @return true if a file descriptor has been created successfully
     * @return false otherwise
     */
    bool start();

    /**
     * @brief Accept a connection and return a stream.
     * @note This is a blocking call.
     *
     * @return new stream pointer. Caller is responsible for its destruction.
     */
    TCPStream *accept();

private:
    int lfd_;
    uint16_t port_;
    bool listening_;
    std::string address_;
};

} // namespace net
} // namespace kb