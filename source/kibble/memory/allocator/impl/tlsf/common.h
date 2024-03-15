#pragma once

#include <cstddef>

#include "assert/assert.h"
#include "memory/allocator/impl/tlsf/bit.h"

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
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2", nullptr);
    return (x + (alignment - 1)) & ~(alignment - 1);
}

inline size_t align_down(size_t x, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2", nullptr);
    return x - (x & (alignment - 1));
}

inline void* align_ptr(const void* ptr, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2", nullptr);
    const std::ptrdiff_t aligned =
        (std::ptrdiff_t(ptr) + (std::ptrdiff_t(alignment) - 1)) & ~(std::ptrdiff_t(alignment) - 1);
    return reinterpret_cast<void*>(aligned);
}

// * TLSF utils
void mapping_insert(size_t size, int& fli, int& sli)
{
    if (size < k_small_block_size)
    {
        // Store small blocks in first list
        fli = 0;
        sli = int(size) / int(k_small_block_size / k_sl_index_count);
    }
    else
    {
        fli = fls_size_t(size);
        sli = (int(size) >> (fli - int(k_sl_index_count_log2))) ^ int(1 << k_sl_index_count_log2);
        fli -= (k_fl_index_shift - 1);
    }
}

// This version rounds up to the next block size (for allocations)
void mapping_search(size_t size, int& fli, int& sli)
{
    if (size >= k_small_block_size)
    {
        const size_t round = (1 << (size_t(fls_size_t(size)) - k_sl_index_count_log2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

} // namespace kb::memory::tlsf