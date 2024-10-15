#include "kibble/memory/allocator/linear_allocator.h"
#include "config.h"
#include "kibble/assert/assert.h"
#include "kibble/memory/arena_base.h"
#include "kibble/memory/heap_area.h"
#include "kibble/memory/util/alignment.h"

namespace kb
{
namespace memory
{

LinearAllocator::LinearAllocator(const MemoryArenaBase* arena, HeapArea& area, uint32_t, std::size_t size)
{
    std::pair<void*, void*> range = area.require_slab(size, arena);

    begin_ = static_cast<uint8_t*>(range.first);
    end_ = static_cast<uint8_t*>(range.second);
    head_ = 0;
}

void* LinearAllocator::allocate(std::size_t size, std::size_t alignment, std::size_t offset)
{
    uint8_t* current = begin_ + head_;

    // We want the user pointer (at current+offset) to be aligned.
    // Check if alignment is required. If so, find the next aligned memory address.
    std::size_t padding = alignment_padding(current + offset, alignment);

    // Out of memory
    if (current + padding + size > end_)
    {
        K_ASSERT(false, "[LinearAllocator] Out of memory!\n  -> padded size: {}, exceeded by: {}", padding + size,
                 (std::size_t(current) + padding + size) - std::size_t(end_));
        return nullptr;
    }

    // Mark padding area
#ifdef K_USE_MEM_MARK_PADDING
    std::fill(current, current + padding, k_alignment_padding_mark);
#endif

    head_ += padding + size;
    return current + padding;
}

} // namespace memory
} // namespace kb