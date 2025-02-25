#include "kibble/random/uuid.h"
#include "kibble/platform/arch.h"
#include "kibble/random/impl/endian.h"

#include <emmintrin.h>
#include <smmintrin.h>

namespace kb
{
namespace UUIDv4
{

// Forward declarations
namespace impl::sse2
{
// Implemented in impl/uuid_sse2.cpp
/// @internal @brief Converts a 128-bits unsigned int to an UUIDv4 string representation.
void m128itos(__m128i x, char* mem);
/// @internal @brief Converts an UUIDv4 string representation to a 128-bits unsigned int.
__m128i stom128i(const char* mem);
} // namespace impl::sse2

namespace impl::avx2
{
// Implemented in impl/uuid_avx2.cpp
void m128itos(__m128i x, char* mem);
__m128i stom128i(const char* mem);
} // namespace impl::avx2

// CPU dispatch using function-local static initialization to sidestep the static
// initialization order fiasco with CPUInfo
inline void m128itos(__m128i x, char* mem)
{
    static auto dispatch_func = []() -> void (*)(__m128i, char*) {
        return CPUInfo::has_ISA_AVX2() ? impl::avx2::m128itos : impl::sse2::m128itos;
    }();

    dispatch_func(x, mem);
}

inline __m128i stom128i(const char* mem)
{
    static auto dispatch_func = []() -> __m128i (*)(const char*) {
        return CPUInfo::has_ISA_AVX2() ? impl::avx2::stom128i : impl::sse2::stom128i;
    }();

    return dispatch_func(mem);
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
constexpr unsigned long long k_u_and_mask = 0xFFFFFFFFFFFFFF3FULL;
constexpr unsigned long long k_l_and_mask = 0xFF0FFFFFFFFFFFFFULL;
constexpr unsigned long long k_u_or_mask = 0x0000000000000080ULL;
constexpr unsigned long long k_l_or_mask = 0x0040000000000000ULL;

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

    // SSE4.1
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