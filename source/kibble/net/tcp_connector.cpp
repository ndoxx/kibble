#include "kibble/net/tcp_connector.h"
#include "kibble/net/impl/wsa_guard.h"
#include "kibble/net/tcp_stream.h"

#include <cstring>
#if defined(K_PLATFORM_LINUX)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
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

/**
 * @internal
 * @brief Resolve a hostname to an IPv4 address using getaddrinfo.
 */
bool resolve_host(const std::string& hostname, in_addr* addr)
{
    addrinfo* res = nullptr;
    int result = getaddrinfo(hostname.c_str(), nullptr, nullptr, &res);
    if (result == 0)
    {
        memcpy(addr, &(reinterpret_cast<sockaddr_in*>(res->ai_addr))->sin_addr, sizeof(in_addr));
        freeaddrinfo(res);
        return true;
    }
    return false;
}

inline bool socket_is_invalid(detail::socket_t s)
{
    return s == detail::k_invalid_socket;
}

inline void close_socket(detail::socket_t s)
{
#if defined(K_PLATFORM_LINUX)
    close(s);
#elif defined(K_PLATFORM_WINDOWS)
    closesocket(s);
#endif
}

} // namespace

std::expected<std::unique_ptr<TCPStream>, NetError> TCPConnector::connect(const std::string& server, uint16_t port)
{
    detail::WSAGuard::init();

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    // Prefer DNS resolution; fall back to treating the string as a literal IP.
    if (!resolve_host(server, &address.sin_addr))
    {
        if (inet_pton(AF_INET, server.c_str(), &address.sin_addr) != 1)
        {
#if defined(K_PLATFORM_LINUX)
            return std::unexpected(NetError{EINVAL, "connect(): failed to resolve host"});
#elif defined(K_PLATFORM_WINDOWS)
            return std::unexpected(NetError{WSAEINVAL, "connect(): failed to resolve host"});
#endif
        }
    }

    detail::socket_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_is_invalid(fd))
    {
        return std::unexpected(NetError::current("socket()"));
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        auto err = NetError::current("connect()");
        close_socket(fd);
        return std::unexpected(std::move(err));
    }

    return std::unique_ptr<TCPStream>(new TCPStream(fd, &address));
}

} // namespace kb::net