#include "memory/pool_allocator.h"
#include "assert/assert.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"

#include <iostream>

#define ALLOCATOR_PADDING_MAGIC

namespace kb
{
namespace memory
{

PoolAllocator::PoolAllocator(HeapArea &area, std::size_t node_size, std::size_t max_nodes, const char *debug_name)
{
    init(area, node_size, max_nodes, debug_name);
}

void PoolAllocator::init(HeapArea &area, std::size_t node_size, std::size_t max_nodes, const char *debug_name)
{
    node_size_ = node_size;
    max_nodes_ = max_nodes;
    void *begin = area.require_pool_block(node_size_, max_nodes_, debug_name);
    begin_ = static_cast<uint8_t *>(begin);
    end_ = begin_ + max_nodes_ * node_size_;
    free_list_.init(begin_, node_size_, max_nodes_, 0, 0);
}

void *PoolAllocator::allocate([[maybe_unused]] std::size_t size, std::size_t alignment, std::size_t offset)
{
    uint8_t *next = static_cast<uint8_t *>(free_list_.acquire());
    // We want the user pointer (at next+offset) to be aligned.
    // Check if alignment is required. If so, find the next aligned memory address.
    std::size_t padding = 0;
    if (alignment && std::size_t(next + offset) % alignment)
        padding = utils::alignment_padding(next + offset, alignment);

    K_ASSERT(padding + size <= node_size_, "[PoolAllocator] Allocation size does not fit initial requirement.", nullptr)
        .watch(padding + size)
        .watch(node_size_);

    // Mark padding area
#ifdef ALLOCATOR_PADDING_MAGIC
    std::fill(next, next + padding, 0xd0);
#endif

    // std::cout << "Alloc: " << std::hex << size_t(next) << std::dec << " padding= " << padding << std::endl;

    return next + padding;
}

void PoolAllocator::deallocate(void *ptr)
{
    // Get unaligned address
    size_t offset = size_t(static_cast<uint8_t *>(ptr) - begin_); // Distance in bytes to beginning of the block
    size_t padding = offset % node_size_; // Distance in bytes to beginning of the node = padding
    // std::cout << "Dealloc: " << std::hex << size_t((uint8_t*)ptr-padding) << std::dec << " padding= " << padding <<
    // std::endl;

    free_list_.release(static_cast<uint8_t *>(ptr) - padding);
}

} // namespace memory
} // namespace kb