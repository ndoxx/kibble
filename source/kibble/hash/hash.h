#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace kb
{
namespace hakz
{

/**
 * @brief Return a reversible hash of the input integer
 *
 * @param x
 * @return constexpr uint32_t
 */
constexpr uint32_t rev_hash_32(uint32_t x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

/**
 * @brief Gets back the original integer that was hashed using rev_hash_32()
 *
 * @param x
 * @return constexpr uint32_t
 */
constexpr uint32_t rev_unhash_32(uint32_t x)
{
    x = ((x >> 16) ^ x) * 0x119de1f3;
    x = ((x >> 16) ^ x) * 0x119de1f3;
    x = (x >> 16) ^ x;
    return x;
}

/**
 * @brief Return a reversible hash of the input integer
 *
 * @param x
 * @return constexpr uint64_t
 */
constexpr uint64_t rev_hash_64(uint64_t x)
{
    x = (x ^ (x >> 30)) * uint64_t(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * uint64_t(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

/**
 * @brief Gets back the original integer that was hashed using rev_hash_64()
 *
 * @param x
 * @return constexpr uint64_t
 */
constexpr uint64_t rev_unhash_64(uint64_t x)
{
    x = (x ^ (x >> 31) ^ (x >> 62)) * uint64_t(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * uint64_t(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);
    return x;
}

/**
 * @brief Hash a float such that very close numbers are grouped under the same hash.
 * The hash presicion is a 3 LSB epsilon. This implementation only works with
 * little endian systems at the moment
 *
 * @param f
 * @return constexpr uint32_t
 */
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
namespace detail
{
// These magic numbers are the binary expansions of an irrational number (the golden ratio)
static constexpr uint64_t PHI_EXP_32 = 0x9E3779B9;         // 2^32/phi
static constexpr uint64_t PHI_EXP_64 = 0x9E3779B97F4A7C15; // 2^64/phi
inline void update_hash_seed_internal(size_t& seed, size_t next_value)
{
    seed ^= next_value + PHI_EXP_64 + (seed << 6) + (seed >> 2);
}
} // namespace detail

/**
 * @brief Convenience hasher struct to allow easy pair hashing, provided the two types in the pair are hashable.
 *
 */
struct pair_hash
{
    /**
     * @brief XOR-combine the hashes of the two elements of a pair.
     *
     * @tparam T1 Type of the first element
     * @tparam T2 Type of the second element
     * @param pair The pair to hash
     * @return std::size_t
     */
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& pair) const
    {
        std::size_t seed = 0;
        detail::update_hash_seed_internal(seed, std::hash<T1>()(pair.first));
        detail::update_hash_seed_internal(seed, std::hash<T2>()(pair.second));
        return seed;
    }
};

/**
 * @brief Convenience hasher struct to allow easy vec3 epsilon hashing
 *
 * @tparam Vec3T Type of the 3D vector
 */
template <typename Vec3T>
struct vec3_hash
{
    /**
     * @brief Return a combined hash of all the vector's elements.
     * Two vectors that are very close will give the same hash. I mostly use this in my mesh logic to detect multiple
     * occurrences of the same spatial point and associate them to a unique primitive.
     *
     * @param vec The input vector
     * @return std::size_t
     */
    std::size_t operator()(const Vec3T& vec) const
    {
        std::size_t seed = 0;

        // Combine component hashes to obtain a position hash
        // Similar to Boost's hash_combine function
        for (int ii = 0; ii < 3; ++ii)
        {
            detail::update_hash_seed_internal(seed, hakz::epsilon_hash(vec[ii]));
        }

        return seed; // Two epsilon-distant vectors will share a common hash
    }
};
} // namespace kh

// ----- constexpr string hash facilities -----

namespace detail
{
// FNV-1a constants
static constexpr unsigned long long basis = 14695981039346656037ULL;
static constexpr unsigned long long prime = 1099511628211ULL;

// compile-time hash helper functions
static constexpr unsigned long long hash_one(char c, const char* remain, unsigned long long value)
{
    return c == 0 ? value : hash_one(remain[0], remain + 1, (value ^ static_cast<unsigned long long>(c)) * prime);
}
static constexpr unsigned long long hash_one(std::string_view sv, size_t idx, unsigned long long value)
{
    return idx == sv.size() ? value : hash_one(sv, idx + 1, (value ^ static_cast<unsigned long long>(sv[idx])) * prime);
}

inline void hash_combine([[maybe_unused]] std::size_t& seed)
{
}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
    std::hash<T> hasher;
    update_hash_seed_internal(seed, hasher(v));
    hash_combine(seed, rest...);
}
} // namespace detail

#define MAKE_HASHABLE(type, ...)                                                                                       \
    namespace std                                                                                                      \
    {                                                                                                                  \
    template <>                                                                                                        \
    struct hash<type>                                                                                                  \
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

/**
 * @brief Compile-time FNV-1a string hash
 *
 * @param str Input c-string
 * @return constexpr kb::hash_t
 */
constexpr kb::hash_t H_(const char* str)
{
    return kb::detail::hash_one(str[0], str + 1, kb::detail::basis);
}

/**
 * @brief Compile-time FNV-1a string hash
 *
 * @param sv Input string_view
 * @return constexpr kb::hash_t
 */
constexpr kb::hash_t H_(std::string_view sv)
{
    return kb::detail::hash_one(sv, 0, kb::detail::basis);
}

/**
 * @brief FNV-1a string hash
 *
 * @param str Input string
 * @return constexpr kb::hash_t
 */
constexpr kb::hash_t H_(const std::string& str)
{
    return H_(str.c_str());
}

/**
 * @brief String litteral operator for terse string hash expressions.
 * The syntax "hello"_h is equivalent to calling H_("hello")
 *
 */
constexpr kb::hash_t operator"" _h(const char* internstr, size_t)
{
    return H_(internstr);
}

/**
 * @brief XOR-combines two string hashes
 *
 * @param first First hash
 * @param second Second hash
 * @return kb::hash_t
 */
inline kb::hash_t HCOMBINE_(kb::hash_t first, kb::hash_t second)
{
    return (first ^ second) * kb::detail::prime;
}

/**
 * @brief XOR-combine a list of hashed strings
 *
 * @param hashes Vector containing string hashes
 * @return kb::hash_t
 */
inline kb::hash_t HCOMBINE_(const std::vector<kb::hash_t>& hashes)
{
    kb::hash_t ret = hashes[0];
    for (size_t ii = 1; ii < hashes.size(); ++ii)
    {
        ret = HCOMBINE_(ret, hashes[ii]);
    }
    return ret;
}