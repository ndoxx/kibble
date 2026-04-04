#pragma once

#include "kibble/platform/platform.h"

#if defined(K_PLATFORM_WINDOWS)
#include <cstdint>
#endif

// ---------------------------------------------------------------------------
// Platform socket / ssize type aliases
// On Linux:   socket_t is int (file descriptor), ssize_t mirrors the POSIX type.
// On Windows: SOCKET is typedef'd to UINT_PTR (an unsigned pointer-sized int).
//             SSIZE_T is typedef'd to INT_PTR (a signed pointer-sized int).
//             We reproduce those without pulling in winsock2.h / BaseTsd.h.
// ---------------------------------------------------------------------------
namespace kb::net::detail
{

#if defined(K_PLATFORM_LINUX)

using socket_t = int;
using ssize_t = long; // matches POSIX ssize_t on all LP64 Linux targets

inline constexpr socket_t k_invalid_socket = -1;

#elif defined(K_PLATFORM_WINDOWS)

// SOCKET  == UINT_PTR, SSIZE_T == INT_PTR - both are pointer-sized.
using socket_t = uintptr_t;
using ssize_t = intptr_t;

inline constexpr socket_t k_invalid_socket = ~socket_t{0}; // INVALID_SOCKET == (SOCKET)(~0)

#endif

} // namespace kb::net::detail
