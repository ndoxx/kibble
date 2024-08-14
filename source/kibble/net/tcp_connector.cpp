#include "kibble/net/tcp_connector.h"
#include "kibble/net/tcp_stream.h"

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace kb
{
namespace net
{

static bool resolve_host(const std::string& hostname, in_addr* addr)
{
    addrinfo* res;

    int result = getaddrinfo(hostname.c_str(), NULL, NULL, &res);
    if (result == 0)
    {
        memcpy(addr, &(reinterpret_cast<sockaddr_in*>(res->ai_addr))->sin_addr, sizeof(in_addr));
        freeaddrinfo(res);
        return true;
    }

    return false;
}

TCPStream* TCPConnector::connect(const std::string& server, uint16_t port)
{
    sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    // Try to convert DNS host name. If it fails, we assume the string was an IP address.
    // TODO: parse the string for a valid IP
    if (resolve_host(server, &address.sin_addr))
    {
        inet_pton(PF_INET, server.c_str(), &address.sin_addr);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        return nullptr;
    }

    return new TCPStream(fd, &address);
}

} // namespace net
} // namespace kb