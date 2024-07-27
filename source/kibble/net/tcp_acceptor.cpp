#include "tcp_acceptor.h"
#include "tcp_stream.h"
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

TCPAcceptor::TCPAcceptor(uint16_t port, const char* address)
    : lfd_(0), port_(port), listening_(false), address_(address)
{
}

TCPAcceptor::~TCPAcceptor()
{
    if (lfd_)
    {
        close(lfd_);
    }
}

bool TCPAcceptor::start()
{
    // If already listening, return
    if (listening_)
    {
        return 0;
    }

    // Create a listening socket
    lfd_ = socket(PF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = PF_INET;
    address.sin_port = htons(port_);
    if (!address_.empty())
    {
        inet_pton(PF_INET, address_.c_str(), &address.sin_addr);
    }
    else
    {
        address.sin_addr.s_addr = INADDR_ANY;
    }

    int optval = 1;
    setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    int result = bind(lfd_, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result != 0)
    {
        perror("bind() failed");
        return false;
    }

    result = listen(lfd_, 5);
    if (result != 0)
    {
        perror("listen() failed");
        return false;
    }

    listening_ = true;
    return true;
}

TCPStream* TCPAcceptor::accept()
{
    if (!listening_)
    {
        return nullptr;
    }

    sockaddr_in address;
    socklen_t len = sizeof(address);
    memset(&address, 0, sizeof(address));
    int fd = ::accept(lfd_, reinterpret_cast<sockaddr*>(&address), &len);
    if (fd < 0)
    {
        perror("accept() failed");
        return nullptr;
    }

    return new TCPStream(fd, &address);
}

} // namespace net
} // namespace kb