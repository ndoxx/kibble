#pragma once

#include <functional>
#include <vector>

namespace kb
{

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
            erwin::detail::hash_combine(ret, __VA_ARGS__);                                                             \
            return ret;                                                                                                \
        }                                                                                                              \
    };                                                                                                                 \
    }

using hash_t = unsigned long long;
// compile-time hash
static constexpr hash_t H_(const char* str) { return detail::hash_one(str[0], str + 1, detail::basis); }

// string literal expression
static constexpr hash_t operator"" _h(const char* internstr, size_t) { return H_(internstr); }

inline hash_t HCOMBINE_(hash_t first, hash_t second) { return (first ^ second) * detail::prime; }

inline hash_t HCOMBINE_(const std::vector<hash_t>& hashes)
{
    hash_t ret = hashes[0];
    for(size_t ii = 1; ii < hashes.size(); ++ii)
        ret = HCOMBINE_(ret, hashes[ii]);
    return ret;
}

} // namespace kb

// #include "core/intern_string.h" // for HRESOLVE
