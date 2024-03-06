#pragma once

#include <cstdint>
#include <string>

namespace kb::memory::utils
{

/**
 * @brief Calculate an alignment padding.
 * The padding size returned is such that `(base_address  + padding) % alignment == 0`.
 *
 * @param base_address the base address
 * @param alignment alignment size
 * @return padding size required to align `base_address  + padding`
 */
[[maybe_unused]] static inline std::size_t alignment_padding(uint8_t* base_address, std::size_t alignment)
{
    size_t base_address_sz = reinterpret_cast<std::size_t>(base_address);

    std::size_t multiplier = (base_address_sz / alignment) + 1;
    std::size_t aligned_address = multiplier * alignment;
    std::size_t padding = aligned_address - base_address_sz;

    return padding;
}

/**
 * @brief Round a number to the nearest multiple of another number.
 * Useful to calculate total node size of an aligned object.
 *
 * @param base the base size
 * @param multiple the multiple
 * @return a number that is always a multiple of 'multiple', greater than or equal to base.
 */
std::size_t round_up(std::size_t base, std::size_t multiple);

// TODO:
// [ ] Move the hex dump free functions to a proper class

/**
 * @brief Return a human readable size string.
 *
 * @param bytes input size in bytes
 * @return a formatted string with the size converted to the maximal size unit multiple followed by a suffix, like "3.92
 * GB" or "12 kB" for example.
 */
std::string human_size(std::size_t bytes);

} // namespace kb::memory::utils