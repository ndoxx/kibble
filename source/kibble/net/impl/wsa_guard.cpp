#include "kibble/net/impl/wsa_guard.h"

#if defined(K_PLATFORM_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <iostream>
#include <winsock2.h>

namespace kb::net::detail
{

WSAGuard::WSAGuard()
{
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        // WSAStartup failing is catastrophic and pre-dates any NetError machinery.
        std::cerr << "WSAStartup failed: " << result << "\n";
    }
}

WSAGuard::~WSAGuard()
{
    WSACleanup();
}

void WSAGuard::init()
{
    static WSAGuard guard; // constructed once, destroyed at program exit
}

} // namespace kb::net::detail

#endif // K_PLATFORM_WINDOWS