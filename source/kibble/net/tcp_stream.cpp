#include "tcp_stream.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

namespace kb
{
namespace net
{

constexpr unsigned int k_max_buf_len = 4096;

TCPStream::TCPStream(int fd, void *address_in) : fd_(fd)
{
    sockaddr_in *address = static_cast<sockaddr_in *>(address_in);
    char ip[50];
    inet_ntop(PF_INET, static_cast<in_addr_t *>(&(address->sin_addr.s_addr)), ip, sizeof(ip) - 1);
    peer_ip_ = ip;
    peer_port_ = ntohs(address->sin_port);
}

TCPStream::~TCPStream()
{
    close(fd_);
}

ssize_t TCPStream::send(const char *buffer, size_t len)
{
    return write(fd_, buffer, len);
}

ssize_t TCPStream::receive(char *buffer, size_t len)
{
    return read(fd_, buffer, len);
}

void TCPStream::receive(std::string &msg)
{
    std::vector<char> buffer(k_max_buf_len);
    ssize_t nbytes = 0;

    do
    {
        nbytes = recv(fd_, &buffer[0], k_max_buf_len, 0);
        // append string from buffer.
        if (nbytes != -1)
            msg.append(buffer.cbegin(), buffer.cend());
        else
            std::cerr << "TCPStream receive error." << std::endl;
    } while (nbytes == k_max_buf_len);
}

} // namespace net
} // namespace kb