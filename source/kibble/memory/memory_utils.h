#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "../logger/common.h"

namespace kb
{
namespace memory
{
namespace utils
{

/**
 * @brief Calculate an alignment padding.
 * The padding size returned is such that `(base_address  + padding) % alignment == 0`.
 *
 * @param base_address the base address
 * @param alignment alignment size
 * @return padding size required to align `base_address  + padding`
 */
[[maybe_unused]] static inline std::size_t alignment_padding(uint8_t *base_address, std::size_t alignment)
{
    size_t base_address_sz = reinterpret_cast<std::size_t>(base_address);

    std::size_t multiplier = (base_address_sz / alignment) + 1;
    std::size_t aligned_address = multiplier * alignment;
    std::size_t padding = aligned_address - base_address_sz;

    return padding;
}

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

} // namespace utils

/**
 * @brief Configure a highlight that will be applied by the hex dump each type a specific 32b word is encountered.
 *
 * @param word 32b word to highlight
 * @param wcb background color
 */
void hex_dump_highlight(uint32_t word, const kb::KB_ &wcb);

/**
 * @brief Remove all previously configured hex dump highlights
 *
 */
void hex_dump_clear_highlights();

/**
 * @brief Serialize a hex dump to an input stream.
 *
 * @param stream stream to dump to
 * @param ptr pointer to the beginning of the memory to dump
 * @param size size of the block to dump in bytes
 * @param title title of the hex dump
 */
void hex_dump(std::ostream &stream, const void *ptr, std::size_t size, const std::string &title = "");

} // namespace memory

/**
 * @brief Convert a size in bytes to a size in bytes (does nothing)
 *
 */
constexpr size_t operator"" _B(unsigned long long size)
{
    return size;
}

/**
 * @brief Convert a size in kilobytes to a size in bytes
 *
 */
constexpr size_t operator"" _kB(unsigned long long size)
{
    return 1024 * size;
}

/**
 * @brief Convert a size in megabytes to a size in bytes
 *
 */
constexpr size_t operator"" _MB(unsigned long long size)
{
    return 1048576 * size;
}

/**
 * @brief Convert a size in gigabytes to a size in bytes
 *
 */
constexpr size_t operator"" _GB(unsigned long long size)
{
    return 1073741824 * size;
}

} // namespace kb