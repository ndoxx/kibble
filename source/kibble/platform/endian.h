#pragma once

#if defined(__GLIBC__) || defined(__GNU_LIBRARY__) || defined(__ANDROID__)
#include <endian.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <machine/endian.h>
#elif defined(BSD) || defined(_SYSTYPE_BSD)
#if defined(__OpenBSD__)
#include <machine/endian.h>
#else
#include <sys/endian.h>
#endif
#endif

#if defined(__BYTE_ORDER)
#if defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#define BIGENDIAN
#elif defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#define LITTLEENDIAN
#endif
#elif defined(_BYTE_ORDER)
#if defined(_BIG_ENDIAN) && (_BYTE_ORDER == _BIG_ENDIAN)
#define BIGENDIAN
#elif defined(_LITTLE_ENDIAN) && (_BYTE_ORDER == _LITTLE_ENDIAN)
#define LITTLEENDIAN
#endif
#elif defined(__BIG_ENDIAN__)
#define BIGENDIAN
#elif defined(__LITTLE_ENDIAN__)
#define LITTLEENDIAN
#else
#if defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) ||   \
    defined(__MIPSEL__) || defined(__ia64__) || defined(_IA64) || defined(__IA64__) || defined(__ia64) ||              \
    defined(_M_IA64) || defined(__itanium__) || defined(i386) || defined(__i386__) || defined(__i486__) ||             \
    defined(__i586__) || defined(__i686__) || defined(__i386) || defined(_M_IX86) || defined(_X86_) ||                 \
    defined(__THW_INTEL__) || defined(__I86__) || defined(__INTEL__) || defined(__x86_64) || defined(__x86_64__) ||    \
    defined(__amd64__) || defined(__amd64) || defined(_M_X64) || defined(__bfin__) || defined(__BFIN__) ||             \
    defined(bfin) || defined(BFIN)

#define LITTLEENDIAN
#elif defined(__m68k__) || defined(M68000) || defined(__hppa__) || defined(__hppa) || defined(__HPPA__) ||             \
    defined(__sparc__) || defined(__sparc) || defined(__370__) || defined(__THW_370__) || defined(__s390__) ||         \
    defined(__s390x__) || defined(__SYSC_ZARCH__)

#define BIGENDIAN

#elif defined(__arm__) || defined(__arm64) || defined(__thumb__) || defined(__TARGET_ARCH_ARM) ||                      \
    defined(__TARGET_ARCH_THUMB) || defined(__ARM_ARCH) || defined(_M_ARM) || defined(_M_ARM64)

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)

#define LITTLEENDIAN

#else
#error "Cannot determine system endianness."
#endif
#endif
#endif

#include <cstring> // memcpy
#include <stdint.h>

#if defined(_MSC_VER)
#include <stdlib.h> // _byteswap_* intrinsics
#endif

#if defined(__SSSE3__)
#include <emmintrin.h> // SSE2  - __m128i
#include <smmintrin.h> // SSE4.1
#include <tmmintrin.h> // SSSE3 - _mm_shuffle_epi8
#endif

#if defined(__AVX2__)
#include <immintrin.h> // AVX2  - __m256i, _mm256_shuffle_epi8
#endif

/**
 * @brief Byte-swap helpers for converting between big-endian and little-endian
 *        representations for scalar integer and SIMD types.
 *
 * On little-endian systems all conversion functions compile to no-ops.
 * On big-endian systems they perform the appropriate byte reversal using the
 * best available compiler intrinsic.
 *
 * SIMD variants (@ref betole_bytes128, @ref betole_bytes256) are only available when the
 * translation unit is compiled with SSSE3 (`-mssse3`) or AVX2 (`-mavx2`) respectively.
 * Note that @ref betole_bytes256 reverses bytes within each 128-bit lane independently
 * due to AVX2 lane boundary constraints - see its documentation for details.
 */
namespace endian
{

/// @internal
/// @brief Implementation details - not part of the public API.
namespace detail
{

// ---------------------------------------------------------------------------
// Unsigned byte-swap - compiler intrinsic selection
// ---------------------------------------------------------------------------

#if defined(BIGENDIAN)

#if defined(__INTEL_COMPILER) || defined(__ICC)

// clang-format off
/// @internal @brief Reverses the byte order of a 16-bit unsigned integer (Intel compiler intrinsic).
inline uint16_t bswap(uint16_t x) { return _bswap16(x); }
/// @internal @brief Reverses the byte order of a 32-bit unsigned integer (Intel compiler intrinsic).
inline uint32_t bswap(uint32_t x) { return _bswap(x); }
/// @internal @brief Reverses the byte order of a 64-bit unsigned integer (Intel compiler intrinsic).
inline uint64_t bswap(uint64_t x) { return _bswap64(x); }
// clang-format on

#elif defined(__GNUC__) // GCC and Clang

// clang-format off
/// @internal @brief Reverses the byte order of a 16-bit unsigned integer (GCC/Clang built-in).
inline uint16_t bswap(uint16_t x) { return __builtin_bswap16(x); }
/// @internal @brief Reverses the byte order of a 32-bit unsigned integer (GCC/Clang built-in).
inline uint32_t bswap(uint32_t x) { return __builtin_bswap32(x); }
/// @internal @brief Reverses the byte order of a 64-bit unsigned integer (GCC/Clang built-in).
inline uint64_t bswap(uint64_t x) { return __builtin_bswap64(x); }
// clang-format on

#elif defined(_MSC_VER)

// clang-format off
/// @internal @brief Reverses the byte order of a 16-bit unsigned integer (MSVC intrinsic).
inline uint16_t bswap(uint16_t x) { return _byteswap_ushort(x); }
/// @internal @brief Reverses the byte order of a 32-bit unsigned integer (MSVC intrinsic).
inline uint32_t bswap(uint32_t x) { return _byteswap_ulong(x); }
/// @internal @brief Reverses the byte order of a 64-bit unsigned integer (MSVC intrinsic).
inline uint64_t bswap(uint64_t x) { return _byteswap_uint64(x); }
// clang-format on

#else // Portable fallback

/// @internal @brief Reverses the byte order of a 16-bit unsigned integer (portable fallback).
inline uint16_t bswap(uint16_t x)
{
    return static_cast<uint16_t>(((x & 0xFF00u) >> 8u) | ((x & 0x00FFu) << 8u));
}

/// @internal @brief Reverses the byte order of a 32-bit unsigned integer (portable fallback).
inline uint32_t bswap(uint32_t x)
{
    return ((x & 0xFF000000u) >> 24u) | ((x & 0x00FF0000u) >> 8u) | ((x & 0x0000FF00u) << 8u) |
           ((x & 0x000000FFu) << 24u);
}

/// @internal @brief Reverses the byte order of a 64-bit unsigned integer (portable fallback).
inline uint64_t bswap(uint64_t x)
{
    return ((x & 0xFF00000000000000u) >> 56u) | ((x & 0x00FF000000000000u) >> 40u) |
           ((x & 0x0000FF0000000000u) >> 24u) | ((x & 0x000000FF00000000u) >> 8u) | ((x & 0x00000000FF000000u) << 8u) |
           ((x & 0x0000000000FF0000u) << 24u) | ((x & 0x000000000000FF00u) << 40u) | ((x & 0x00000000000000FFu) << 56u);
}

#endif // compiler selection

// ---------------------------------------------------------------------------
// Signed byte-swap
// ---------------------------------------------------------------------------

/**
 * @internal
 * @brief Reverses the byte order of a signed integer by reinterpreting its bits
 *        as the corresponding unsigned type, swapping, then reinterpreting back.
 *
 * @tparam Signed   Signed integer type (e.g. `int32_t`).
 * @tparam Unsigned Unsigned integer type of the same width (e.g. `uint32_t`).
 * @param x Value whose bytes are to be reversed.
 * @return  @p x with its bytes in reversed order.
 */
template <typename Signed, typename Unsigned>
inline Signed bswap_signed(Signed x)
{
    Unsigned u;
    memcpy(&u, &x, sizeof(u));
    u = bswap(u);
    Signed s;
    memcpy(&s, &u, sizeof(s));
    return s;
}

// clang-format off
/// @internal @brief Reverses the byte order of a 16-bit signed integer.
inline int16_t bswap(int16_t x) { return bswap_signed<int16_t, uint16_t>(x); }
/// @internal @brief Reverses the byte order of a 32-bit signed integer.
inline int32_t bswap(int32_t x) { return bswap_signed<int32_t, uint32_t>(x); }
/// @internal @brief Reverses the byte order of a 64-bit signed integer.
inline int64_t bswap(int64_t x) { return bswap_signed<int64_t, uint64_t>(x); }
// clang-format on

// ---------------------------------------------------------------------------
// SIMD byte-swap
// ---------------------------------------------------------------------------

#if defined(__SSSE3__)
/**
 * @internal
 * @brief Reverses the byte order of the entire 128-bit register, treating it as
 *        a contiguous 16-byte sequence. Byte 0 becomes byte 15 and vice versa.
 *        Requires SSSE3.
 * @param value 128-bit SSE register to byte-swap.
 * @return      @p value with all 16 bytes in reversed order.
 */
inline __m128i bswap(__m128i value)
{
    const __m128i shuffle = _mm_set_epi64x(0x0001020304050607, 0x08090a0b0c0d0e0f);
    return _mm_shuffle_epi8(value, shuffle);
}
#endif // __SSSE3__

#if defined(__AVX2__)
/**
 * @internal
 * @brief Reverses the byte order within each 128-bit lane of a 256-bit AVX register.
 *
 * Due to the lane boundary constraint of @c _mm256_shuffle_epi8, bytes cannot
 * cross the 128-bit boundary. Each 16-byte half is reversed independently:
 * byte 0 becomes byte 15, and byte 16 becomes byte 31, but the two halves are
 * not swapped relative to each other. Requires AVX2.
 *
 * @param value 256-bit AVX register to byte-swap.
 * @return      @p value with bytes reversed within each 128-bit lane.
 */
inline __m256i bswap(__m256i value)
{
    const __m256i shuffle =
        _mm256_set_epi64x(0x0001020304050607, 0x08090a0b0c0d0e0f, 0x0001020304050607, 0x08090a0b0c0d0e0f);
    return _mm256_shuffle_epi8(value, shuffle);
}
#endif // __AVX2__

#else // LITTLEENDIAN - all swaps are no-ops

// clang-format off
/// @internal @brief No-op on little-endian systems.
inline uint16_t bswap(uint16_t x) { return x; }
/// @internal @brief No-op on little-endian systems.
inline uint32_t bswap(uint32_t x) { return x; }
/// @internal @brief No-op on little-endian systems.
inline uint64_t bswap(uint64_t x) { return x; }
/// @internal @brief No-op on little-endian systems.
inline int16_t  bswap(int16_t x)  { return x; }
/// @internal @brief No-op on little-endian systems.
inline int32_t  bswap(int32_t x)  { return x; }
/// @internal @brief No-op on little-endian systems.
inline int64_t  bswap(int64_t x)  { return x; }
#if defined(__SSSE3__)
/// @internal @brief No-op on little-endian systems.
inline __m128i  bswap(__m128i x)  { return x; }
#endif
#if defined(__AVX2__)
/// @internal @brief No-op on little-endian systems.
inline __m256i  bswap(__m256i x)  { return x; }
#endif
// clang-format on

#endif // BIGENDIAN / LITTLEENDIAN

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @defgroup endian_convert Big-endian ↔ little-endian conversion
 * @{
 *
 * Each function converts a value from big-endian to little-endian byte order
 * (or equivalently, from little-endian to big-endian - the operation is
 * symmetric).  On little-endian hosts all functions are no-ops and compile
 * away entirely.
 */

// clang-format off
// Unsigned
/// @brief Converts a 16-bit unsigned integer between big-endian and little-endian byte order.
inline uint16_t betoleu16(uint16_t x) { return detail::bswap(x); }
/// @brief Converts a 32-bit unsigned integer between big-endian and little-endian byte order.
inline uint32_t betoleu32(uint32_t x) { return detail::bswap(x); }
/// @brief Converts a 64-bit unsigned integer between big-endian and little-endian byte order.
inline uint64_t betoleu64(uint64_t x) { return detail::bswap(x); }

// Signed
/// @brief Converts a 16-bit signed integer between big-endian and little-endian byte order.
inline int16_t betolei16(int16_t x) { return detail::bswap(x); }
/// @brief Converts a 32-bit signed integer between big-endian and little-endian byte order.
inline int32_t betolei32(int32_t x) { return detail::bswap(x); }
/// @brief Converts a 64-bit signed integer between big-endian and little-endian byte order.
inline int64_t betolei64(int64_t x) { return detail::bswap(x); }
// clang-format on

// SIMD
#if defined(__SSSE3__)
/**
 * @brief Reverses the byte order of a 128-bit SSE register, treating it as a
 *        contiguous 16-byte sequence. Byte 0 becomes byte 15 and vice versa.
 *        Requires SSSE3.
 */
inline __m128i betole_bytes128(__m128i x)
{
    return detail::bswap(x);
}
#endif
#if defined(__AVX2__)
/**
 * @brief Reverses the byte order within each 128-bit lane of a 256-bit AVX register.
 *
 * Due to the lane boundary constraint of @c _mm256_shuffle_epi8, each 16-byte
 * half is reversed independently - bytes do not cross the 128-bit boundary,
 * and the two halves are not swapped relative to each other. Requires AVX2.
 */
inline __m256i betole_bytes256(__m256i x)
{
    return detail::bswap(x);
}
#endif

/** @} */ // endian_convert

} // namespace endian