#pragma once

#include <cstdint>
#include <string>

#include "../../math/color.h"

namespace kb::memory::util
{

/**
 * @brief Configure a highlight that will be applied by the hex dump each type a specific 32b word is encountered.
 *
 * @param word 32b word to highlight
 * @param color background color
 */
void hex_dump_highlight(uint32_t word, math::argb32_t color);

/**
 * @brief Remove all previously configured hex dump highlights
 *
 */
void hex_dump_clear_highlights();

/**
 * @brief Print hex dump to standard output
 *
 * @param ptr pointer to the beginning of the memory to dump
 * @param size size of the block to dump in bytes
 * @param title title of the hex dump
 */
void hex_dump(const void* ptr, std::size_t size, const std::string& title = "");

} // namespace kb::memory::util