#include "kibble/net/tcp_acceptor.h"
#include "kibble/net/impl/wsa_guard.h"
#include "kibble/net/tcp_stream.h"

#include <cstring>
#if defined(K_PLATFORM_LINUX)
#include <arpa/inet.h>
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

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

TCPAcceptor::TCPAcceptor(uint16_t port, const char* address)
    : lfd_(detail::k_invalid_socket), port_(port), listening_(false), address_(address)
{
    detail::WSAGuard::init();
}

TCPAcceptor::~TCPAcceptor()
{
    if (!socket_is_invalid(lfd_))
    {
        close_socket(lfd_);
    }
}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

std::expected<void, NetError> TCPAcceptor::start()
{
    if (listening_)
    {
        return {}; // idempotent
    }

    lfd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_is_invalid(lfd_))
    {
        return std::unexpected(NetError::current("socket()"));
    }

    // Allow fast server restarts without waiting for TIME_WAIT to expire.
#if defined(K_PLATFORM_LINUX)
    int optval = 1;
    setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#elif defined(K_PLATFORM_WINDOWS)
    BOOL optval = TRUE;
    setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optval), sizeof(optval));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    if (!address_.empty())
    {
        inet_pton(AF_INET, address_.c_str(), &address.sin_addr);
    }
    else
    {
        address.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(lfd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        auto err = NetError::current("bind()");
        close_socket(lfd_);
        lfd_ = detail::k_invalid_socket;
        return std::unexpected(std::move(err));
    }

    if (listen(lfd_, 5) != 0)
    {
        auto err = NetError::current("listen()");
        close_socket(lfd_);
        lfd_ = detail::k_invalid_socket;
        return std::unexpected(std::move(err));
    }

    listening_ = true;
    return {};
}

// ---------------------------------------------------------------------------
// accept()
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<TCPStream>, NetError> TCPAcceptor::accept()
{
    if (!listening_)
    {
#if defined(K_PLATFORM_LINUX)
        return std::unexpected(NetError{ENOTCONN, "accept()"});
#elif defined(K_PLATFORM_WINDOWS)
        return std::unexpected(NetError{WSAENOTCONN, "accept()"});
#endif
    }

#if defined(K_PLATFORM_LINUX)
    using socklen = socklen_t;
#elif defined(K_PLATFORM_WINDOWS)
    using socklen = int;
#endif

    sockaddr_in address{};
    socklen len = sizeof(address);
    detail::socket_t fd = ::accept(lfd_, reinterpret_cast<sockaddr*>(&address), &len);

    if (socket_is_invalid(fd))
    {
        return std::unexpected(NetError::current("accept()"));
    }

    // TCPStream constructor is private; use a friend-accessible helper via make_unique equivalent.
    // Since TCPStream's constructor is private and TCPAcceptor is a friend, we construct directly.
    return std::unique_ptr<TCPStream>(new TCPStream(fd, &address));
}

} // namespace kb::net