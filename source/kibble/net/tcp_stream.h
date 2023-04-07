#pragma once

#include <string>

namespace kb
{
namespace net
{

/**
 * @brief Used for bidirectionnal communication between a client and a server.
 * It represents an active connection, created either actively by a TCPConnector, or passively by a TCPAcceptor.
 *
 * @note This object is non-copyable and its constructor is private. Only TCPAcceptor and TCPConnector can create it.
 * @note At the moment, only a linux implementation is available.
 */
class TCPStream
{
public:
    friend class TCPAcceptor;
    friend class TCPConnector;

    /**
     * @brief Close the file descriptor and destroy the stream.
     *
     */
    ~TCPStream();

    /**
     * @brief Get the remote port of this connection.
     *
     * @return port number
     */
    inline uint16_t get_peer_port() const
    {
        return peer_port_;
    }

    /**
     * @brief Get the remote IP address of this connection.
     *
     * @return the IP address as a string
     */
    inline const std::string& get_peer_ip() const
    {
        return peer_ip_;
    }

    /**
     * @brief Send a data buffer.
     *
     * @param buffer pointer to the data buffer
     * @param len size of the data to transmit in bytes
     * @return size of data that was written, or -1 on error
     */
    ssize_t send(const char* buffer, size_t len);

    /**
     * @brief Receive data from the peer and copy it to a buffer.
     *
     * @param buffer pointer to the data buffer
     * @param len maximum amount of data to read
     * @return the number of bytes read, 0 if EOF reached, -1 on error
     */
    ssize_t receive(char* buffer, size_t len);

    /**
     * @brief Convenience function to send a string.
     *
     * @param msg string to send
     * @return size of data that was written, or -1 on error
     */
    inline ssize_t send(const std::string& msg)
    {
        return send(msg.c_str(), msg.size());
    }

    /**
     * @brief Receive data and put it into a string.
     *
     * @param msg target string
     */
    void receive(std::string& msg);

private:
    TCPStream() = default;
    TCPStream(const TCPStream& stream) = delete;
    TCPStream(int fd, void* address_in);

private:
    int fd_;              // Socket file descriptor
    uint16_t peer_port_;  // Remote port for this connection
    std::string peer_ip_; // Remote IP for this connection
};

} // namespace net
} // namespace kb