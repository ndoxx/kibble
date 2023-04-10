#pragma once

/**
 * @brief Functions to encode to and decode from Morton codes
 *
 * This is a rework of a 19 month old fork of libmorton (by Forceflow)
 *  -> https://github.com/Forceflow/libmorton
 *  -> Licence is MIT
 *
 *  Changes are:
 *  -> Stripped away code I don't care about
 *      -> I only kept LUT and BMI2 implementations as they are the fastest, according to
 *         the author's benchmarks
 *  -> API
 *      -> Using tuple return types instead of output arguments
 *      -> Using templated functions
 *      -> Optional GLM wrappers
 *  -> Algo
 *      -> Choice of implementation (LUT/AVX2-BMI) via header selection (ENABLE_INTRIN_MORTON compilation option)
 *      -> LUTs are generated programatically at compile time
 *  -> Style & coding standards
 *      -> Naming conventions
 *      -> Code compiles with the most paranoid set of warnings
 *
 * @author ndx
 * @version 0.2
 * @date 2023-04-09
 *
 * @copyright Copyright (c) 2023
 */

#include <cstdint>
#include <tuple>

#ifdef USE_GLM
#include <glm/glm.hpp>
#include <type_traits>
#endif

namespace kb::morton
{

/**
 * @brief Morton encode a 2D coordinate
 *
 * @warning Input and output types are the same. It is the caller's responsibility to make sure that both x and y use
 * only half the available bits of the underlying type (LSB). For example, when working with uint64_t, x and y only use
 * the first 32 bits.
 *
 * @tparam T either uint32_t or uint64_t
 * @param x
 * @param y
 * @return the morton code
 */
template <typename T>
T encode(const T x, const T y);

/**
 * @brief Morton encode a 3D coordinate
 *
 * @warning Input and output types are the same. It is the caller's responsibility to make sure that x, y and z use
 * at most a third of the available bits of the underlying type (LSB). For example, when working with uint64_t, x, y and
 * z only use the first 21 bits.
 *
 *
 * @tparam T either uint32_t or uint64_t
 * @param x
 * @param y
 * @param z
 * @return the morton code
 */
template <typename T>
T encode(const T x, const T y, const T z);

/**
 * @brief Decode a Morton code to a 2D coordinate
 *
 * @tparam T either uint32_t or uint64_t
 * @param key the Morton code
 * @return the x and y coordinates in a tuple
 */
template <typename T>
std::tuple<T, T> decode_2d(const T key);

/**
 * @brief Decode a Morton code to a 3D coordinate
 *
 * @tparam T either uint32_t or uint64_t
 * @param key the Morton code
 * @return the x, y and z coordinates in a tuple
 */
template <typename T>
std::tuple<T, T, T> decode_3d(const T key);

// * Specializations

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

// * GLM wrappers
#ifdef USE_GLM

namespace detail
{
/**
 * @internal
 * @brief Helper to form the unsigned type corresponding to a glm::vec underlying type
 *
 * @tparam VecT
 */
template <typename VecT>
using unsigned_value_t = std::make_unsigned_t<typename VecT::value_type>;
} // namespace detail

/**
 * @brief Morton encode a 2D or 3D coordinate
 *
 * @tparam VecT can be i32vec2, i64vec2, i32vec3 or i64vec3
 * @param val the coordinate vector
 * @return the Morton code
 */
template <typename VecT>
inline detail::unsigned_value_t<VecT> encode(const VecT& val)
{
    if constexpr (VecT::length() == 2)
    {
        return encode(detail::unsigned_value_t<VecT>(val[0]), detail::unsigned_value_t<VecT>(val[1]));
    }
    else if constexpr (VecT::length() == 3)
    {
        return encode(detail::unsigned_value_t<VecT>(val[0]), detail::unsigned_value_t<VecT>(val[1]),
                      detail::unsigned_value_t<VecT>(val[2]));
    }
}

/**
 * @brief Decode a Morton code to either a 2D or 3D coordinate vector
 * 
 * @tparam VecT can be i32vec2, i64vec2, i32vec3 or i64vec3
 * @param key the Morton code
 * @return the coordinate vector
 */
template <typename VecT>
inline VecT decode(const detail::unsigned_value_t<VecT> key)
{
    if constexpr (VecT::length() == 2)
    {
        auto&& [x, y] = decode_2d(key);
        return {x, y};
    }
    else if constexpr (VecT::length() == 3)
    {
        auto&& [x, y, z] = decode_3d(key);
        return {x, y, z};
    }
}

#endif

} // namespace kb::morton