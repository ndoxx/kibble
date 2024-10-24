#pragma once

#include <cstddef>

#include "kibble/assert/assert.h"
#include "kibble/memory/allocator/tlsf/impl/bit.h" // Leave this here

using std::size_t;

namespace kb::memory::tlsf
{

// * Constants
// log2 of linear subdivisions of block sizes. 4 - 5 are typical values. Larger values require more memory in the
// control structure.
constexpr size_t k_sl_index_count_log2 = 5ull;
// All allocation sizes and addresses are aligned to 8 bytes
constexpr size_t k_align_size_log2 = 3ull;
constexpr size_t k_align_size = 1ull << k_align_size_log2;
// Number of elements in the bitmaps
constexpr size_t k_fl_index_max = 32ull;
constexpr size_t k_sl_index_count = 1ull << k_sl_index_count_log2;
constexpr size_t k_fl_index_shift = k_sl_index_count_log2 + k_align_size_log2;
constexpr size_t k_fl_index_count = k_fl_index_max - k_fl_index_shift + 1ull;
constexpr size_t k_small_block_size = 1ull << k_fl_index_shift;

// * Alignment utils
inline bool is_pow2(size_t x)
{
    return (x & (x - 1)) == 0;
}

inline size_t align_up(size_t x, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2, but got: {}", alignment);
    return (x + (alignment - 1)) & ~(alignment - 1);
}

inline size_t align_down(size_t x, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2, but got: {}", alignment);
    return x - (x & (alignment - 1));
}

inline void* align_ptr(const void* ptr, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2, but got: {}", alignment);
    const std::ptrdiff_t aligned =
        (std::ptrdiff_t(ptr) + (std::ptrdiff_t(alignment) - 1)) & ~(std::ptrdiff_t(alignment) - 1);
    return reinterpret_cast<void*>(aligned);
}

// * TLSF utils
void mapping_insert(size_t size, int& fli, int& sli);

// This version rounds up to the next block size (for allocations)
void mapping_search(size_t size, int& fli, int& sli);

} // namespace kb::memory::tlsf