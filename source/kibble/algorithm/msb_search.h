#pragma once

/**
 * @file msb_search.h
 * @author ndx
 * @brief Leiserson - Prokop - Randall algorithm implementation
 *
 * -> to find the most significant bit in a word using De Bruijn multiplication for fast log2
 * -> See: Using de Bruijn Sequences to Index a 1 in a Computer Word (Charles E. Leiserson, Harald Prokop Keith & H.
 * Randall) http://supertech.csail.mit.edu/papers/debruijn.pdf
 *
 * @version 0.1
 * @date 2023-04-08
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <cstdint>

namespace kb
{
namespace detail
{
template <typename T>
struct MSB_Data
{
};
template <>
struct MSB_Data<uint32_t>
{
    static constexpr std::size_t k_mul_de_bruijn_bit[] = {0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
                                                          8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31};
    static constexpr uint32_t k_max_iter = 16;
    static constexpr uint32_t k_magic = 0x07C4ACDD;
    static constexpr uint32_t k_shift = 27;
};
template <>
struct MSB_Data<uint64_t>
{
    // Specialization for 64b integers, following Niklas B.'s answer in:
    // https://stackoverflow.com/questions/21888140/de-bruijn-algorithm-binary-digit-count-64bits-c-sharp
    static constexpr std::size_t k_mul_de_bruijn_bit[] = {
        0,  47, 0, 0,  30, 0,  14, 50, 0,  62, 4,  0, 0,  0,  18, 0,  22, 27, 0,  0,  0,  39, 35, 45, 0, 12,
        0,  0,  0, 33, 0,  57, 0,  59, 1,  42, 54, 0, 0,  0,  49, 61, 3,  0,  17, 26, 0,  38, 44, 0,  0, 32,
        56, 0,  0, 53, 0,  48, 0,  16, 0,  0,  31, 0, 52, 0,  15, 0,  0,  51, 0,  0,  0,  63, 5,  6,  7, 0,
        8,  0,  0, 0,  19, 9,  0,  0,  23, 0,  28, 0, 0,  20, 0,  10, 0,  0,  40, 0,  24, 36, 0,  46, 0, 29,
        13, 0,  0, 0,  0,  21, 0,  0,  34, 11, 0,  0, 0,  58, 41, 0,  0,  60, 2,  25, 37, 43, 0,  55};
    static constexpr uint64_t k_max_iter = 32;
    static constexpr uint64_t k_magic = 0x6C04F118E9966F6B;
    static constexpr uint64_t k_shift = 57;
};
} // namespace detail

template <typename T>
std::size_t msb_search(T v)
{
    for (T ii = 1; ii <= detail::MSB_Data<T>::k_max_iter; ii *= 2)
    {
        v |= v >> ii;
    }

    return detail::MSB_Data<T>::k_mul_de_bruijn_bit[(v * detail::MSB_Data<T>::k_magic) >> detail::MSB_Data<T>::k_shift];
}

} // namespace kb