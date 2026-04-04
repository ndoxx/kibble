#pragma once

#include "kibble/platform/platform.h" // IWYU pragma: keep

namespace kb::net::detail
{

#if defined(K_PLATFORM_WINDOWS)

/**
 * @brief RAII wrapper that calls WSAStartup once per process on first use
 *        and WSACleanup when the process exits.
 *
 * Obtain the singleton via WSAGuard::init() before any socket call.
 * The guard object itself lives until program termination (Meyer's singleton).
 */
class WSAGuard
{
public:
    /// Ensure Winsock is initialised. Safe to call multiple times.
    static void init();

private:
    WSAGuard();
    ~WSAGuard();
};

#else // K_PLATFORM_LINUX - Winsock does not exist; init() is a no-op.

/**
 * @brief Stub for Linux that does not need WSAStartup.
 *
 */
struct WSAGuard
{
    static void init()
    {
    }
};

#endif

} // namespace kb::net::detail