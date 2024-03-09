#pragma once

#include <cstdint>
#include <new>

namespace kb::memory
{

// Size of a cache line -> controlling alignment prevents false sharing
#ifdef __cpp_lib_hardware_interference_size
constexpr size_t k_cache_line_size = std::hardware_destructive_interference_size;
#else
// 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
constexpr size_t k_cache_line_size = 64;
#endif

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

#define L1_ALIGN alignas(kb::memory::k_cache_line_size)

} // namespace kb::memory