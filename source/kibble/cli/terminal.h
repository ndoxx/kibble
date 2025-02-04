#pragma once
#include <cstdint>
#include <utility>

namespace kb
{
namespace cli
{

/**
 * @brief [OS-dependent] Retrieve the respective number of columns and rows in the terminal
 *
 * @return std::pair<uint32_t, uint32_t> width and height
 */
std::pair<uint32_t, uint32_t> get_terminal_size();

/**
 * @brief [OS-dependent] Enable ANSI support in a Windows terminal, does nothing on other platforms
 * 
 */
void enable_terminal_ANSI_support();

} // namespace cli
} // namespace kb