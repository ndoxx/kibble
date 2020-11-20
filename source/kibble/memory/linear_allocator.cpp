#include "memory/linear_allocator.h"
#include "memory/memory_utils.h"
#include "memory/heap_area.h"
#include "assert/assert.h"

#include <iostream>

#define ALLOCATOR_PADDING_MAGIC

namespace kb
{
namespace memory
{

LinearAllocator::LinearAllocator(HeapArea& area, std::size_t size, const char* debug_name)
{
    init(area, size, debug_name);
}

void LinearAllocator::init(HeapArea& area, std::size_t size, const char* debug_name)
{
    std::pair<void*,void*> range = area.require_block(size, debug_name);

    begin_ = static_cast<uint8_t*>(range.first);
    end_ = static_cast<uint8_t*>(range.second);
    current_offset_ = 0;
}

void* LinearAllocator::allocate(std::size_t size, std::size_t alignment, std::size_t offset)
{
	uint8_t* current = begin_ + current_offset_;

    // We want the user pointer (at current+offset) to be aligned.
    // Check if alignment is required. If so, find the next aligned memory address.
    std::size_t padding = 0;
    if(alignment && std::size_t(current+offset) % alignment)
        padding = utils::alignment_padding(current+offset, alignment);

	// Out of memory
    if(current + padding + size > end_)
    {
#ifndef K_DEBUG
    	K_ASSERT(false, "[LinearAllocator] Out of memory!");
#endif
        return nullptr;
    }

    // Mark padding area
#ifdef ALLOCATOR_PADDING_MAGIC
    std::fill(current, current + padding, 0xd0);
#endif

    current_offset_ += padding + size;
    return current + padding;
}


} // namespace memory
} // namespace kb