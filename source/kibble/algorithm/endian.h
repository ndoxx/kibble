#pragma once

#include <climits>
#include <cstdint>
#include <utility>

namespace kb
{

namespace detail
{
template <typename T, std::size_t... N> constexpr T bswap_impl(T i, std::index_sequence<N...>)
{
    return static_cast<T>((((i >> N * CHAR_BIT & std::uint8_t(-1)) << (sizeof(T) - 1 - N) * CHAR_BIT) | ...));
}
} // namespace detail

/**
 * @brief Compile-time endianness swap
 *
 * Based on http://stackoverflow.com/a/36937049.
 * Clang and gcc manage to generate a bswap assembly instruction from this.
 * Usage:\n
 * `bswap<std::uint16_t>(0x1234u)                 ->   0x3412u`\n
 * `bswap<std::uint64_t>(0x0123456789abcdefULL)   ->   0xefcdab8967452301ULL`\n
 *
 * @tparam T Type of variable to endian-swap
 * @tparam U (deduced) unsigned type associated to T
 * @param i Variable to endian-swap
 * @return constexpr U
 */
template <typename T, typename U = std::make_unsigned_t<T>> constexpr U bswap(T i)
{
    return detail::bswap_impl<U>(i, std::make_index_sequence<sizeof(T)>{});
}

} // namespace kb