#pragma once

#include <cstdint>
#include <new>

namespace kb::th
{

// Size of a cache line -> controlling alignment prevents false sharing
#ifdef __cpp_lib_hardware_interference_size
[[maybe_unused]] static constexpr size_t k_cache_line_size = std::hardware_destructive_interference_size;
#else
// 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
[[maybe_unused]] static constexpr size_t k_cache_line_size = 64;
#endif

#define PAGE_ALIGN alignas(k_cache_line_size)

} // namespace kb::th