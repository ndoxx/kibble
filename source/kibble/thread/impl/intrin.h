#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>

namespace kb
{
namespace intrin
{
/**
 * @internal
 * @brief Spinlock implementation using an intrinsic
 *
 */
[[maybe_unused]] static inline void spin__() noexcept
{
    _mm_pause();
}
} // namespace intrin
} // namespace kb

#else
#error Unsupported architecture
#endif
