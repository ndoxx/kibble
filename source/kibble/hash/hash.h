#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace kb
{
namespace hakz
{

// Reversible integer hashes
constexpr uint32_t rev_hash_32(uint32_t x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}
constexpr uint32_t rev_unhash_32(uint32_t x)
{
    x = ((x >> 16) ^ x) * 0x119de1f3;
    x = ((x >> 16) ^ x) * 0x119de1f3;
    x = (x >> 16) ^ x;
    return x;
}
constexpr uint64_t rev_hash_64(uint64_t x)
{
    x = (x ^ (x >> 30)) * uint64_t(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * uint64_t(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}
constexpr uint64_t rev_unhash_64(uint64_t x)
{
    x = (x ^ (x >> 31) ^ (x >> 62)) * uint64_t(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * uint64_t(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);
    return x;
}

// Epsilon float hash => group very close numbers under the same hash
constexpr uint32_t epsilon_hash(float f)
{
    union // Used to reinterpret float mantissa as an unsigned integer...
    {
        float f_;
        uint32_t u_;
    };
    f_ = f;

    // ... with a 3 LSB epsilon (floats whose mantissas are only 3 bits different will share a common hash)
    return u_ & 0xfffffff8; // BEWARE: Depends on endianness
}
} // namespace hakz

namespace kh
{
struct pair_hash
{
    template <typename T1, typename T2> std::size_t operator()(const std::pair<T1, T2>& pair) const
    {
        return std::hash<T1>()(pair.first) ^ std::hash<T1>()(pair.second);
    }
};

template <typename Vec3T> struct vec3_hash
{
    std::size_t operator()(const Vec3T& vec) const
    {
        std::size_t seed = 0;

        // Combine component hashes to obtain a position hash
        // Similar to Boost's hash_combine function
        for(int ii = 0; ii < 3; ++ii)
            seed ^= hakz::epsilon_hash(vec[ii]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed; // Two epsilon-distant vectors will share a common hash
    }
};
} // namespace kh

// ----- constexpr string hash facilities -----
// NOTE: hashing algorithm used is FNV-1a

namespace detail
{
// FNV-1a constants
static constexpr unsigned long long basis = 14695981039346656037ULL;
static constexpr unsigned long long prime = 1099511628211ULL;

// compile-time hash helper function
static constexpr unsigned long long hash_one(char c, const char* remain, unsigned long long value)
{
    return c == 0 ? value : hash_one(remain[0], remain + 1, (value ^ static_cast<unsigned long long>(c)) * prime);
}

inline void hash_combine([[maybe_unused]] std::size_t& seed) {}

template <typename T, typename... Rest> inline void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    hash_combine(seed, rest...);
}
} // namespace detail

#define MAKE_HASHABLE(type, ...)                                                                                       \
    namespace std                                                                                                      \
    {                                                                                                                  \
    template <> struct hash<type>                                                                                      \
    {                                                                                                                  \
        std::size_t operator()(const type& t) const                                                                    \
        {                                                                                                              \
            std::size_t ret = 0;                                                                                       \
            kb::detail::hash_combine(ret, __VA_ARGS__);                                                                \
            return ret;                                                                                                \
        }                                                                                                              \
    };                                                                                                                 \
    }

using hash_t = unsigned long long;

} // namespace kb

// compile-time hash
constexpr kb::hash_t H_(const char* str) { return kb::detail::hash_one(str[0], str + 1, kb::detail::basis); }
constexpr kb::hash_t H_(const std::string& str) { return H_(str.c_str()); }

// string literal expression
constexpr kb::hash_t operator"" _h(const char* internstr, size_t) { return H_(internstr); }

inline kb::hash_t HCOMBINE_(kb::hash_t first, kb::hash_t second) { return (first ^ second) * kb::detail::prime; }

inline kb::hash_t HCOMBINE_(const std::vector<kb::hash_t>& hashes)
{
    kb::hash_t ret = hashes[0];
    for(size_t ii = 1; ii < hashes.size(); ++ii)
        ret = HCOMBINE_(ret, hashes[ii]);
    return ret;
}