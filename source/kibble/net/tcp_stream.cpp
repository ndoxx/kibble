#include "kibble/net/tcp_stream.h"
#include "kibble/net/impl/wsa_guard.h"

#include <vector>
#if defined(K_PLATFORM_LINUX)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#elif defined(K_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace kb::net
{

namespace
{
constexpr unsigned int k_max_buf_len = 4096;
} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

TCPStream::TCPStream(detail::socket_t fd, void* address_in) : fd_(fd)
{
    detail::WSAGuard::init();

    auto* address = static_cast<sockaddr_in*>(address_in);
    char ip[50];
    inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip) - 1);
    peer_ip_ = ip;
    peer_port_ = ntohs(address->sin_port);
}

TCPStream::~TCPStream()
{
    if (fd_ != detail::k_invalid_socket)
    {
#if defined(K_PLATFORM_LINUX)
        close(fd_);
#elif defined(K_PLATFORM_WINDOWS)
        closesocket(fd_);
#endif
    }
}

// ---------------------------------------------------------------------------
// I/O
// ---------------------------------------------------------------------------

std::expected<detail::ssize_t, NetError> TCPStream::send(const char* buffer, size_t len)
{
#if defined(K_PLATFORM_LINUX)
    detail::ssize_t n = write(fd_, buffer, len);
#elif defined(K_PLATFORM_WINDOWS)
    detail::ssize_t n = ::send(fd_, buffer, static_cast<int>(len), 0);
#endif
    if (n < 0)
    {
        return std::unexpected(NetError::current("send()"));
    }
    return n;
}

std::expected<detail::ssize_t, NetError> TCPStream::receive(char* buffer, size_t len)
{
#if defined(K_PLATFORM_LINUX)
    detail::ssize_t n = read(fd_, buffer, len);
#elif defined(K_PLATFORM_WINDOWS)
    detail::ssize_t n = ::recv(fd_, buffer, static_cast<int>(len), 0);
#endif
    if (n < 0)
    {
        return std::unexpected(NetError::current("receive()"));
    }
    return n; // 0 == connection closed gracefully - valid, not an error
}

std::expected<void, NetError> TCPStream::receive_exact(char* buffer, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
#if defined(K_PLATFORM_LINUX)
        detail::ssize_t n = read(fd_, buffer + total, len - total);
#elif defined(K_PLATFORM_WINDOWS)
        detail::ssize_t n = ::recv(fd_, buffer + total, static_cast<int>(len - total), 0);
#endif
        if (n <= 0)
        {
            // n == 0 -> connection closed before all bytes arrived
            // n <  0 -> socket error
            if (n == 0)
            {
                // Synthesise a "connection closed" error (errno ECONNRESET / WSAECONNRESET)
#if defined(K_PLATFORM_LINUX)
                return std::unexpected(NetError{ECONNRESET, "receive_exact()"});
#elif defined(K_PLATFORM_WINDOWS)
                return std::unexpected(NetError{WSAECONNRESET, "receive_exact()"});
#endif
            }
            return std::unexpected(NetError::current("receive_exact()"));
        }
        total += static_cast<size_t>(n);
    }
    return {};
}

std::expected<void, NetError> TCPStream::receive(std::string& msg)
{
    std::vector<char> buffer(k_max_buf_len);
    detail::ssize_t nbytes = 0;

    do
    {
#if defined(K_PLATFORM_LINUX)
        nbytes = recv(fd_, buffer.data(), k_max_buf_len, 0);
#elif defined(K_PLATFORM_WINDOWS)
        nbytes = ::recv(fd_, buffer.data(), k_max_buf_len, 0);
#endif
        if (nbytes < 0)
        {
            return std::unexpected(NetError::current("receive()"));
        }
        if (nbytes > 0)
        {
            msg.append(buffer.data(), static_cast<size_t>(nbytes));
        }
    } while (nbytes == static_cast<detail::ssize_t>(k_max_buf_len));

    return {};
}

} // namespace kb::net