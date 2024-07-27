#pragma once

/**
 * @file ctti.h
 * @brief Compile-Time Type Information.
 *
 * This code was written as a replacement for parts of the CTTI lib I happen to use quite often :
 * https://github.com/Manu343726/ctti
 * Adapted from: https://bitwizeshift.github.io/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/
 * Changes are:
 *   - I don't append '\n' after the type name, I see no reason to do so.
 *   - Added a type_id() function that computes a compile-time hash of the type name.
 *   - Extended type_name and type_id overload sets to work with instances as well, using std::decay like lib CTTI does
 *
 * @author ndx
 * @version 0.1
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <array>
#include <string_view>
#include <utility>

#include "../hash/hash.h"

namespace kb
{
namespace ctti
{

namespace detail
{
// Helper function to create a char array from a string_view and a sequence of indices
template <std::size_t... Idxs>
constexpr auto substring_as_array(std::string_view str, std::index_sequence<Idxs...>)
{
    return std::array{str[Idxs]...};
}

// Returns a char array containing the type name of T
template <typename T>
constexpr auto type_name_array()
{
#if defined(__clang__)
    constexpr auto prefix = std::string_view{"[T = "};
    constexpr auto suffix = std::string_view{"]"};
    constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__) && !defined(__clang__)
    constexpr auto prefix = std::string_view{"with T = "};
    constexpr auto suffix = std::string_view{"]"};
    constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
    constexpr auto prefix = std::string_view{"type_name_array<"};
    constexpr auto suffix = std::string_view{">(void)"};
    constexpr auto function = std::string_view{__FUNCSIG__};
#else
#error Unsupported compiler
#endif

    // Parse mangled name
    constexpr auto start = function.find(prefix) + prefix.size();
    constexpr auto end = function.rfind(suffix);
    static_assert(start < end);

    // Type name lies between prefix and suffix
    constexpr auto name = function.substr(start, (end - start));

    // Build char array at compile-time using an index sequence
    return detail::substring_as_array(name, std::make_index_sequence<name.size()>{});
}

// Helper struct to hold a type name array with static lifetime
template <typename T>
struct type_name_holder
{
    static inline constexpr auto value = type_name_array<T>();
};
} // namespace detail

/**
 * @brief Get the name of a type T at compile-time.
 *
 * @tparam T The type to be reflected
 * @return std::string_view
 */
template <typename T>
constexpr auto type_name() -> std::string_view
{
    constexpr auto& value = detail::type_name_holder<T>::value;
    return std::string_view{value.data(), value.size()};
}

/**
 * @brief Get the type name of a value at compile-time. std::decay is applied to
 * T first, use ctti::type_name<decltype(arg)> to preserve references and cv qualifiers.
 *
 * @tparam T The type to be reflected
 * @return std::string_view
 */
template <typename T>
inline constexpr auto type_name(T&&) -> std::string_view
{
    return type_name<typename std::decay<T>::type>();
}

/**
 * @brief Get an integer ID that is unique to the type T. The ID is computed by hashing the
 * string_view returned by type_name()
 *
 * @tparam T The type to be reflected
 * @return Unique id to this type
 */
template <typename T>
constexpr auto type_id() -> hash_t
{
    return H_(type_name<T>());
}

/**
 * @brief Get an integer ID that is unique to the type T. The ID is computed by hashing the
 * string_view returned by type_name()
 *
 * @tparam T The type to be reflected
 * @return Unique id to this type
 */
template <typename T>
inline constexpr auto type_id(T&&) -> hash_t
{
    return type_id<typename std::decay<T>::type>();
}

} // namespace ctti
} // namespace kb