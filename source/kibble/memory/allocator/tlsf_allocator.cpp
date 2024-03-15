#include "memory/allocator/tlsf_allocator.h"
#include "memory/allocator/impl/tlsf/block.h"
#include "memory/allocator/impl/tlsf/control.h"

#include <cstddef>

using std::size_t;

namespace kb::memory
{

using namespace tlsf;

/**
 * @internal
 * @brief Adjust an allocation size request to be aligned to word size
 * and no smaller than internal minimum
 *
 * @param size
 * @param alignment
 * @return size_t
 */
size_t adjust_request_size(size_t size, size_t alignment)
{
    size_t adjusted = 0;
    if (size)
    {
        const size_t aligned = align_up(size, alignment);

        // Aligned sized must not exceed block_size_max or we'll go out of bounds on sl_bitmap
        if (aligned < k_block_size_max)
            adjusted = std::max(aligned, k_block_size_min);
    }
    return adjusted;
}

} // namespace kb::memory