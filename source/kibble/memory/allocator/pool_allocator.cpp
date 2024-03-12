#include "memory/allocator/pool_allocator.h"
#include "assert/assert.h"
#include "math/constexpr_math.h"
#include "memory/config.h"
#include "memory/heap_area.h"
#include "memory/util/alignment.h"

#include <iostream>

namespace kb
{
namespace memory
{

PoolAllocator::PoolAllocator(const char* debug_name, HeapArea& area, uint32_t decoration_size, std::size_t max_nodes,
                             std::size_t user_size, std::size_t max_alignment)
{
    node_size_ = math::round_up_pow2(int32_t(user_size + decoration_size), int32_t(max_alignment));
    max_nodes_ = max_nodes;
    auto range = area.require_block(node_size_ * max_nodes_, debug_name);
    begin_ = static_cast<uint8_t*>(range.first);
    end_ = begin_ + max_nodes_ * node_size_;
    free_list_.init(begin_, node_size_, max_nodes_, 0, 0);
}

void* PoolAllocator::allocate([[maybe_unused]] std::size_t size, std::size_t alignment, std::size_t offset)
{
    uint8_t* next = static_cast<uint8_t*>(free_list_.acquire());
    // We want the user pointer (at next+offset) to be aligned.
    // Check if alignment is required. If so, find the next aligned memory address.
    std::size_t padding = alignment_padding(next + offset, alignment);

    K_ASSERT(padding + size <= node_size_, "[PoolAllocator] Allocation size does not fit initial requirement.", nullptr)
        .watch_var__(padding + size, "requested size")
        .watch_var__(node_size_, "node size")
        .watch_var__(size, "data size")
        .watch_var__(offset, "offset")
        .watch_var__(alignment, "alignment")
        .watch_var__(padding, "padding");

    // Mark padding area
#ifdef K_USE_MEM_MARK_PADDING
    std::fill(next, next + padding, cfg::k_alignment_padding_mark);
#endif

    return next + padding;
}

void PoolAllocator::deallocate(void* ptr)
{
    // Get unaligned address
    size_t offset = size_t(static_cast<uint8_t*>(ptr) - begin_); // Distance in bytes to beginning of the block
    size_t padding = offset % node_size_;                        // Distance in bytes to beginning of the node = padding

    free_list_.release(static_cast<uint8_t*>(ptr) - padding);
}

} // namespace memory
} // namespace kb