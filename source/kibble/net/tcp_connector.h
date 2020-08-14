#pragma once

#include <string>

namespace kb
{
namespace net
{

class TCPStream;

/*
	Factory class that actively connects to a remote machine and
    returns a TCPStream object. 
*/
class TCPConnector
{
public:
    // Generate a TCP stream object by connecting to a server using its name or IP address, and a port
    static TCPStream* connect(const std::string& server, uint16_t port);
};

} // namespace net
} // namespace kb