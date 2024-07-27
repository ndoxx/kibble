#include "random/uuid.h"
#include "random/impl/endian.h"

#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

namespace kb
{
namespace UUIDv4
{

/**
 * @internal
 * @brief Converts a 128-bits unsigned int to an UUIDv4 string representation.
 * Uses SIMD via Intel's AVX2 instruction set.
 *
 * @param x
 * @param mem
 */
inline void m128itos(__m128i x, char* mem)
{
    // Expand each byte in x to two bytes in res
    // i.e. 0x12345678 -> 0x0102030405060708
    // Then translate each byte to its hex ascii representation
    // i.e. 0x0102030405060708 -> 0x3132333435363738
    const __m256i mask = _mm256_set1_epi8(0x0F);
    const __m256i add = _mm256_set1_epi8(0x06);
    const __m256i alpha_mask = _mm256_set1_epi8(0x10);
    const __m256i alpha_offset = _mm256_set1_epi8(0x57);

    __m256i a = _mm256_castsi128_si256(x);
    __m256i as = _mm256_srli_epi64(a, 4);
    __m256i lo = _mm256_unpacklo_epi8(as, a);
    __m128i hi = _mm256_castsi256_si128(_mm256_unpackhi_epi8(as, a));
    __m256i c = _mm256_inserti128_si256(lo, hi, 1);
    __m256i d = _mm256_and_si256(c, mask);
    __m256i alpha = _mm256_slli_epi64(_mm256_and_si256(_mm256_add_epi8(d, add), alpha_mask), 3);
    __m256i offset = _mm256_blendv_epi8(_mm256_slli_epi64(add, 3), alpha_offset, alpha);
    __m256i res = _mm256_add_epi8(d, offset);

    // Add dashes between blocks as specified in RFC-4122
    // 8-4-4-4-12
    const __m256i dash_shuffle = _mm256_set_epi32(int(0x0b0a0908), int(0x07060504), int(0x80030201), int(0x00808080),
                                                  int(0x0d0c800b), int(0x0a090880), int(0x07060504), int(0x03020100));
    const __m256i dash =
        _mm256_set_epi64x(0x0000000000000000ull, 0x2d000000002d0000ull, 0x00002d000000002d, 0x0000000000000000ull);

    __m256i resd = _mm256_shuffle_epi8(res, dash_shuffle);
    resd = _mm256_or_si256(resd, dash);

    _mm256_storeu_si256(reinterpret_cast<__m256i*>(mem), betole256(resd));
    *reinterpret_cast<uint16_t*>(mem + 16) = betole16(uint16_t(_mm256_extract_epi16(res, 7)));
    *reinterpret_cast<uint32_t*>(mem + 32) = betole32(uint32_t(_mm256_extract_epi32(res, 7)));
}

/**
 * @internal Converts an UUIDv4 string representation to a 128-bits unsigned int.
 * @brief Uses SIMD via Intel's AVX2 instruction set.
 *
 * @param mem
 * @return __m128i
 */
inline __m128i stom128i(const char* mem)
{
    // Remove dashes and pack hex ascii bytes in a 256-bits int
    const __m256i dash_shuffle = _mm256_set_epi32(int(0x80808080), int(0x0f0e0d0c), int(0x0b0a0908), int(0x06050403),
                                                  int(0x80800f0e), int(0x0c0b0a09), int(0x07060504), int(0x03020100));

    __m256i x = betole256(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(mem)));
    x = _mm256_shuffle_epi8(x, dash_shuffle);
    x = _mm256_insert_epi16(x, betole16(*reinterpret_cast<const uint16_t*>(mem + 16)), 7);
    x = _mm256_insert_epi32(x, betole32(*reinterpret_cast<const uint32_t*>(mem + 32)), 7);

    // Build a mask to apply a different offset to alphas and digits
    const __m256i sub = _mm256_set1_epi8(0x2F);
    const __m256i mask = _mm256_set1_epi8(0x20);
    const __m256i alpha_offset = _mm256_set1_epi8(0x28);
    const __m256i digits_offset = _mm256_set1_epi8(0x01);
    const __m256i unweave = _mm256_set_epi32(int(0x0f0d0b09), int(0x0e0c0a08), int(0x07050301), int(0x06040200),
                                             int(0x0f0d0b09), int(0x0e0c0a08), int(0x07050301), int(0x06040200));
    const __m256i shift = _mm256_set_epi32(int(0x00000000), int(0x00000004), int(0x00000000), int(0x00000004),
                                           int(0x00000000), int(0x00000004), int(0x00000000), int(0x00000004));

    // Translate ascii bytes to their value
    // i.e. 0x3132333435363738 -> 0x0102030405060708
    // Shift hi-digits
    // i.e. 0x0102030405060708 -> 0x1002300450067008
    // Horizontal add
    // i.e. 0x1002300450067008 -> 0x12345678
    __m256i a = _mm256_sub_epi8(x, sub);
    __m256i alpha = _mm256_slli_epi64(_mm256_and_si256(a, mask), 2);
    __m256i sub_mask = _mm256_blendv_epi8(digits_offset, alpha_offset, alpha);
    a = _mm256_sub_epi8(a, sub_mask);
    a = _mm256_shuffle_epi8(a, unweave);
    a = _mm256_sllv_epi32(a, shift);
    a = _mm256_hadd_epi32(a, _mm256_setzero_si256());
    a = _mm256_permute4x64_epi64(a, 0b00001000);

    return _mm256_castsi256_si128(a);
}

inline UUID UUID_factory(__m128i data)
{
    UUID uuid;
    _mm_store_si128(reinterpret_cast<__m128i*>(uuid.data()), data);
    return uuid;
}

UUID::UUID(const UUID& other)
{
    __m128i x = _mm_load_si128(reinterpret_cast<const __m128i*>(other.data_));
    _mm_store_si128(reinterpret_cast<__m128i*>(data_), x);
}

UUID::UUID(uint64_t x, uint64_t y)
{
    __m128i z = _mm_set_epi64x(static_cast<long long>(x), static_cast<long long>(y));
    _mm_store_si128(reinterpret_cast<__m128i*>(data_), z);
}

UUID::UUID(const uint8_t* bytes)
{
    __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bytes));
    _mm_store_si128(reinterpret_cast<__m128i*>(data_), x);
}

/* Builds an UUID from a byte string (16 bytes long) */
UUID::UUID(const std::string& bytes)
{
    __m128i x = betole128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(bytes.data())));
    _mm_store_si128(reinterpret_cast<__m128i*>(data_), x);
}

UUID::UUID(const char* raw)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(data_), stom128i(raw));
}

/* Static factory to parse an UUID from its string representation */
UUID UUID::from_str_factory(const std::string& s)
{
    return from_str_factory(s.c_str());
}

UUID UUID::from_str_factory(const char* raw)
{
    return UUID_factory(stom128i(raw));
}

// These masks are used to set the UUID version to 4 and variant to 1
constexpr auto k_u_and_mask = 0xFFFFFFFFFFFFFF3Full;
constexpr auto k_l_and_mask = 0xFF0FFFFFFFFFFFFFull;
constexpr auto k_u_or_mask = 0x0000000000000080ull;
constexpr auto k_l_or_mask = 0x0040000000000000ull;

UUID UUID::from_upper_lower(uint64_t upper, uint64_t lower)
{
    const __m128i and_mask = _mm_set_epi64x(static_cast<long long>(k_u_and_mask), static_cast<long long>(k_l_and_mask));
    const __m128i or_mask = _mm_set_epi64x(static_cast<long long>(k_u_or_mask), static_cast<long long>(k_l_or_mask));
    __m128i n = _mm_set_epi64x(static_cast<long long>(upper), static_cast<long long>(lower));
    __m128i uuid = _mm_or_si128(_mm_and_si128(n, and_mask), or_mask);

    return UUID_factory(uuid);
}

UUID& UUID::operator=(const UUID& other)
{
    if (&other == this)
    {
        return *this;
    }

    __m128i x = _mm_load_si128(reinterpret_cast<const __m128i*>(other.data_));
    _mm_store_si128(reinterpret_cast<__m128i*>(data_), x);
    return *this;
}

bool operator==(const UUID& lhs, const UUID& rhs)
{
    __m128i x = _mm_load_si128(reinterpret_cast<const __m128i*>(lhs.data_));
    __m128i y = _mm_load_si128(reinterpret_cast<const __m128i*>(rhs.data_));

    __m128i neq = _mm_xor_si128(x, y);
    return _mm_test_all_zeros(neq, neq);
}

bool operator<(const UUID& lhs, const UUID& rhs)
{
    // There are no trivial 128-bits comparisons in SSE/AVX
    // It's faster to compare two uint64_t
    const uint64_t* x = reinterpret_cast<const uint64_t*>(lhs.data_);
    const uint64_t* y = reinterpret_cast<const uint64_t*>(rhs.data_);
    return *x < *y || (*x == *y && *(x + 1) < *(y + 1));
}

bool operator!=(const UUID& lhs, const UUID& rhs)
{
    return !(lhs == rhs);
}

bool operator>(const UUID& lhs, const UUID& rhs)
{
    return rhs < lhs;
}

bool operator<=(const UUID& lhs, const UUID& rhs)
{
    return !(lhs > rhs);
}

bool operator>=(const UUID& lhs, const UUID& rhs)
{
    return !(lhs < rhs);
}

std::string UUID::bytes() const
{
    std::string mem(sizeof(data_), ' ');
    __m128i x = betole128(_mm_load_si128(reinterpret_cast<const __m128i*>(data_)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(mem.data()), x);
    return mem;
}

std::string UUID::str() const
{
    std::string mem(36, ' ');
    __m128i x = _mm_load_si128(reinterpret_cast<const __m128i*>(data_));
    m128itos(x, mem.data());
    return mem;
}

std::ostream& operator<<(std::ostream& stream, const UUID& uuid)
{
    return stream << uuid.str();
}

std::istream& operator>>(std::istream& stream, UUID& uuid)
{
    std::string s;
    stream >> s;
    uuid = UUID::from_str_factory(s);
    return stream;
}

} // namespace UUIDv4
} // namespace kb