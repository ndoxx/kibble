#pragma once
#include <cstdint>
#include <utility>

namespace kb
{
namespace cli
{

// [OS-dependent] Retrieve the respective number of columns and rows in the terminal
std::pair<uint32_t, uint32_t> get_terminal_size();

} // namespace cli
} // namespace kb