#pragma once

/**
 * @brief Functions to encode to and decode from Morton codes
 *
 * This is a fork of libmorton (by Forceflow)
 *  -> https://github.com/Forceflow/libmorton
 *  -> Licence is MIT
 *
 *  Changes are:
 *  -> Stripped away code I don't care about
 *      -> I only kept LUT and BMI2 implementations as they are the fastest, according to
 *         the author's benchmarks
 *  -> API
 *      -> Using tuple return types instead of output arguments
 *  -> Algo
 *      -> Choice of implementation (LUT/AVX2-BMI) via header selection
 *      -> LUTs are generated programatically at compile time
 *      -> Most significant bit search using Leiserson - Prokop - Randall algorithm
 *  -> Style & coding standards
 *      -> Naming conventions
 *      -> Code compiles with the most paranoid set of warnings
 *
 * @author ndx
 *
 */

#include <cstdint>
#include <tuple>

#ifdef USE_GLM
#include <glm/glm.hpp>
#endif

namespace kb::morton
{

template <typename T>
T encode(const T x, const T y);

template <typename T>
T encode(const T x, const T y, const T z);

template <typename T>
std::tuple<T, T> decode_2d(const T key);

template <typename T>
std::tuple<T, T, T> decode_3d(const T key);

template <>
uint32_t encode(const uint32_t x, const uint32_t y);

template <>
uint32_t encode(const uint32_t x, const uint32_t y, const uint32_t z);

template <>
uint64_t encode(const uint64_t x, const uint64_t y);

template <>
uint64_t encode(const uint64_t x, const uint64_t y, const uint64_t z);

template <>
std::tuple<uint32_t, uint32_t> decode_2d(const uint32_t key);

template <>
std::tuple<uint64_t, uint64_t> decode_2d(const uint64_t key);

template <>
std::tuple<uint32_t, uint32_t, uint32_t> decode_3d(const uint32_t key);

template <>
std::tuple<uint64_t, uint64_t, uint64_t> decode_3d(const uint64_t key);

#ifdef USE_GLM

inline uint32_t encode(const glm::i32vec2& val)
{
    return encode(uint32_t(val[0]), uint32_t(val[1]));
}

inline uint64_t encode(const glm::i64vec2& val)
{
    return encode(uint64_t(val[0]), uint64_t(val[1]));
}

inline uint32_t encode(const glm::i32vec3& val)
{
    return encode(uint32_t(val[0]), uint32_t(val[1]), uint32_t(val[1]));
}

inline uint64_t encode(const glm::i64vec3& val)
{
    return encode(uint64_t(val[0]), uint64_t(val[1]), uint64_t(val[1]));
}

inline glm::i32vec2 decode_i32vec2(const uint32_t key)
{
    auto&& [x, y] = decode_2d(key);
    return {x, y};
}

inline glm::i64vec2 decode_i64vec2(const uint64_t key)
{
    auto&& [x, y] = decode_2d(key);
    return {x, y};
}

inline glm::i32vec3 decode_i32vec3(const uint32_t key)
{
    auto&& [x, y, z] = decode_3d(key);
    return {x, y, z};
}

inline glm::i64vec3 decode_i64vec3(const uint64_t key)
{
    auto&& [x, y, z] = decode_3d(key);
    return {x, y, z};
}

#endif

} // namespace kb::morton