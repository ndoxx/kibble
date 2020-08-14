#pragma once
#include <string>

namespace kb
{
namespace net
{

class TCPStream;

/*
    Factory class that listens on a given port and returns a stream when
    a connection has been established.
*/
class TCPAcceptor
{
public:
    TCPAcceptor(uint16_t port, const char* address = "");
    ~TCPAcceptor();

    // Start listening on port
    bool start();
    // Accept a connection and return a stream
    TCPStream* accept();

private:
    int lfd_;
    uint16_t port_;
    bool listening_;
    std::string address_;
};

} // namespace net
} // namespace kb