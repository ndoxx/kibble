#include "kibble/memory/allocator/tlsf/impl/common.h"

namespace kb::memory::tlsf
{

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