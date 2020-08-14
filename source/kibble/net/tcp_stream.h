#pragma once

#include <string>

namespace kb
{
namespace net
{

/*
    TCPStream represents an active connection, created either actively by a TCPConnector,
    or passively by a TCPAcceptor.
*/
class TCPStream
{
public:
    // Friend factories
    friend class TCPAcceptor;
    friend class TCPConnector;

    ~TCPStream();

    inline uint16_t get_peer_port() const { return peer_port_; }
    inline const std::string& get_peer_ip() const { return peer_ip_; }

    ssize_t send(const char* buffer, size_t len);
    ssize_t receive(char* buffer, size_t len);

    inline ssize_t send(const std::string& msg) { return send(msg.c_str(), msg.size()); }
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