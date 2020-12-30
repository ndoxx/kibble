#pragma once

#include <climits>
#include <cstdint>
#include <utility>

namespace kb
{

// Compile-time endianness swap based on http://stackoverflow.com/a/36937049
// Clang and gcc manage to generate a bswap assembly instruction from this
// usage:
// bswap<std::uint16_t>(0x1234u)                 ->   0x3412u
// bswap<std::uint64_t>(0x0123456789abcdefULL)   ->   0xefcdab8967452301ULL
namespace detail
{
template <typename T, std::size_t... N> constexpr T bswap_impl(T i, std::index_sequence<N...>)
{
    return static_cast<T>((((i >> N * CHAR_BIT & std::uint8_t(-1)) << (sizeof(T) - 1 - N) * CHAR_BIT) | ...));
}
} // namespace detail

template <typename T, typename U = std::make_unsigned_t<T>> constexpr U bswap(T i)
{
    return detail::bswap_impl<U>(i, std::make_index_sequence<sizeof(T)>{});
}

} // namespace kb