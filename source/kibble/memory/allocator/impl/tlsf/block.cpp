#include "memory/allocator/impl/tlsf/block.h"

namespace kb::memory::tlsf
{

void BlockHeader::mark_as_free()
{
    // Link the block to the next block, first
    link_next()->set_prev_free();
    set_free();
}

void BlockHeader::mark_as_used()
{
    get_next()->set_prev_used();
    set_used();
}

BlockHeader* BlockHeader::split(size_t size_request)
{
    // Calculate the amount of space left in the remaining block.
    BlockHeader* remaining = offset_to_block(to_void_ptr(), std::ptrdiff_t(size_request - k_block_header_overhead));
    const size_t size_remaining = block_size() - (size_request + k_block_header_overhead);

    K_ASSERT(remaining->to_void_ptr() == align_ptr(remaining->to_void_ptr(), k_align_size),
             "remaining block not aligned properly", nullptr);
    K_ASSERT(block_size() == size_remaining + size_request + k_block_header_overhead, "remaining block size is invalid",
             nullptr);

    remaining->set_size(size_remaining);

    K_ASSERT(remaining->block_size() >= k_block_size_min, "block split with invalid size", nullptr);

    set_size(size_request);
    remaining->mark_as_free();

    return remaining;
}

} // namespace kb::memory::tlsf