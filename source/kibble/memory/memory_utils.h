#pragma once

#include <cstdint>
#include <ostream>
#include <string>

namespace kb
{

struct WCB;

namespace memory
{
namespace utils
{

[[maybe_unused]] static inline std::size_t alignment_padding(uint8_t* base_address, std::size_t alignment)
{
    size_t base_address_sz = reinterpret_cast<std::size_t>(base_address);

    std::size_t multiplier = (base_address_sz / alignment) + 1;
    std::size_t aligned_address = multiplier * alignment;
    std::size_t padding = aligned_address - base_address_sz;

    return padding;
}

// Return a human readable size string
std::string human_size(std::size_t bytes);

} // namespace utils

void hex_dump_highlight(uint32_t word, const kb::WCB& wcb);
void hex_dump_clear_highlights();
void hex_dump(std::ostream& stream, const void* ptr, std::size_t size, const std::string& title = "");

} // namespace memory

// User literals for bytes multiples
constexpr size_t operator"" _B(unsigned long long size) { return size; }
constexpr size_t operator"" _kB(unsigned long long size) { return 1024 * size; }
constexpr size_t operator"" _MB(unsigned long long size) { return 1048576 * size; }
constexpr size_t operator"" _GB(unsigned long long size) { return 1073741824 * size; }

} // namespace kb