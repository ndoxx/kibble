#pragma once

#include <cstddef>

using std::size_t;

namespace kb::memory::bit
{

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// Fail if not 64B architecture
#if !defined(__alpha__) && !defined(__ia64__) && !defined(__x86_64__) && !defined(_WIN64) && !defined(__LP64__) &&     \
    !defined(__LLP64__)
#error Only x64 architectures are supported
#endif

// Clang
#if defined(__clang__) && __has_builtin(__builtin_ffs) && __has_builtin(__builtin_clz)

inline int ffs(int word)
{
    return __builtin_ffs(word) - 1;
}

inline int fls(int word)
{
    const int bit = word ? 32 - __builtin_clz(static_cast<unsigned int>(word)) : 0;
    return bit - 1;
}

// Use GNU builtins (only retain true-to-god GCC compilers with __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)) && defined(__GNUC_PATCHLEVEL__)

inline int ffs(int word)
{
    return __builtin_ffs(word) - 1;
}

inline int fls(int word)
{
    const int bit = word ? 32 - __builtin_clz(static_cast<unsigned int>(word)) : 0;
    return bit - 1;
}

// Microsoft Visual C++ support on x86/X64 architectures
#elif defined(_MSC_VER) && (_MSC_VER >= 1400) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)

inline int ffs(int word)
{
    unsigned long index;
    return _BitScanForward(&index, word) ? index : -1;
}

inline int fls(int word)
{
    unsigned long index;
    return _BitScanReverse(&index, word) ? index : -1;
}

// Fallback
#else

inline int fls_generic(int word)
{
    int bit = 32;

    if (!word)
        bit -= 1;
    if (!(word & 0xffff0000))
    {
        word <<= 16;
        bit -= 16;
    }
    if (!(word & 0xff000000))
    {
        word <<= 8;
        bit -= 8;
    }
    if (!(word & 0xf0000000))
    {
        word <<= 4;
        bit -= 4;
    }
    if (!(word & 0xc0000000))
    {
        word <<= 2;
        bit -= 2;
    }
    if (!(word & 0x80000000))
    {
        word <<= 1;
        bit -= 1;
    }

    return bit;
}

/* Implement ffs in terms of fls. */
inline int ffs(int word)
{
    return fls_generic(word & (~word + 1)) - 1;
}

inline int fls(int word)
{
    return fls_generic(word) - 1;
}
#endif

inline int fls_size_t(size_t size)
{
    int size_hi = int(size >> 32);
    return bool(size_hi) ? 32 + fls(size_hi) : fls(int(size) & int(0xffffffff));
}

} // namespace kb::memory::bit