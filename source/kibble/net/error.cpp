#include "kibble/net/error.h"
#include "kibble/platform/platform.h"

#include "fmt/core.h"

#if defined(K_PLATFORM_LINUX)
#include <cerrno>
#include <cstring> // strerror_r
#elif defined(K_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#endif

namespace kb::net
{

NetError NetError::current(std::string context)
{
#if defined(K_PLATFORM_LINUX)
    return NetError{errno, std::move(context)};
#elif defined(K_PLATFORM_WINDOWS)
    return NetError{WSAGetLastError(), std::move(context)};
#endif
}

std::string NetError::message() const
{
#if defined(K_PLATFORM_LINUX)
    // strerror_r is thread-safe; use the XSI-compliant version.
    char buf[256];
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE
    int err = strerror_r(code, buf, sizeof(buf));
    (void)err;
    return fmt::format("{} failed [{}]: {}", context, code, buf);
#else
    char* msg = strerror_r(code, buf, sizeof(buf));
    return fmt::format("{} failed [{}]: {}", context, code, msg);
#endif

#elif defined(K_PLATFORM_WINDOWS)
    char buf[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(code),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf) - 1, nullptr);
    // FormatMessage appends "\r\n"; strip trailing whitespace.
    std::string s{buf};
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
    {
        s.pop_back();
    }
    return fmt::format("{} failed [{}]: {}", context, code, s);
#endif
}

WSError WSError::tcp(NetError e)
{
    return WSError{std::move(e)};
}

/// Describe a protocol-level violation.
WSError WSError::protocol(std::string msg)
{
    return WSError{std::move(msg)};
}

[[nodiscard]] std::string WSError::message() const
{
    return std::visit(
        [](const auto& v) -> std::string {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, NetError>)
            {
                return "TCP error: " + v.message();
            }
            else
            {
                return "WS protocol error: " + v;
            }
        },
        cause);
}

std::string WSWarning::message() const
{
    return "WS handshake warning: " + text;
}

} // namespace kb::net