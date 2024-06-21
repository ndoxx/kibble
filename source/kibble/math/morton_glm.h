#pragma once

#include "morton.h"
#include <glm/glm.hpp>
#include <type_traits>

namespace kb::morton
{
namespace detail
{
/**
 * @internal
 * @brief Helper to form the unsigned type corresponding to a glm::vec underlying type
 *
 * @tparam VecT
 */
template <typename VecT>
using unsigned_scalar_t = std::make_unsigned_t<typename VecT::value_type>;
} // namespace detail

/**
 * @brief Morton encode a 2D or 3D coordinate
 *
 * @warning If using a signed integer type, make sure all components are positive.
 *
 * @tparam VecT can be [i|u][32|64]vec[2|3]
 * @param val the coordinate vector
 * @return the Morton code
 */
template <typename VecT, typename = std::enable_if_t<std::is_integral_v<typename VecT::value_type>>>
inline detail::unsigned_scalar_t<VecT> encode(const VecT& val)
{
    if constexpr (VecT::length() == 2)
    {
        return encode(detail::unsigned_scalar_t<VecT>(val[0]), detail::unsigned_scalar_t<VecT>(val[1]));
    }
    else if constexpr (VecT::length() == 3)
    {
        return encode(detail::unsigned_scalar_t<VecT>(val[0]), detail::unsigned_scalar_t<VecT>(val[1]),
                      detail::unsigned_scalar_t<VecT>(val[2]));
    }
}

/**
 * @brief Decode a Morton code to either a 2D or 3D coordinate vector
 *
 * @tparam VecT can be [i|u][32|64]vec[2|3]
 * @param key the Morton code
 * @return the coordinate vector
 */
template <typename VecT, typename = std::enable_if_t<std::is_integral_v<typename VecT::value_type>>>
inline VecT decode(const detail::unsigned_scalar_t<VecT> key)
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

} // namespace kb::morton