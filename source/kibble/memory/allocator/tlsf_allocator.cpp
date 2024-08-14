#include "kibble/memory/allocator/tlsf_allocator.h"
#include "config.h"
#include "kibble/memory/allocator/tlsf/impl/block.h"
#include "kibble/memory/allocator/tlsf/impl/control.h"
#include "kibble/memory/heap_area.h"
#include "kibble/memory/util/alignment.h"

#include "fmt/format.h"
#include <cstddef>

using std::size_t;

namespace kb::memory
{

using namespace tlsf;

// * Constants

/*
    Overhead of the TLSF structures in a given memory block passed to
    tlsf_add_pool, equal to the overhead of a free block and the
    sentinel block.
*/
constexpr size_t k_pool_overhead = 2 * BlockHeader::k_block_header_overhead;

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
        {
            adjusted = std::max(aligned, k_block_size_min);
        }
    }
    return adjusted;
}

TLSFAllocator::TLSFAllocator(const char* debug_name, HeapArea& area, uint32_t, size_t pool_size)
{
    // We want to reserve enough memory for the control structure, the pool, and some leeway for its 8B alignment
    size_t mem_size = sizeof(Control) + alignof(long long) + pool_size;
    std::pair<void*, void*> range = area.require_block(mem_size, debug_name);

    // We know that the first pointer starts at the beginning of a cache line, so it's a fortiori 8B aligned
    // Construct control structure in-place
    control_ = ::new (range.first) Control();

    // Pool starts after control structure, but needs to be aligned
    uint8_t* begin = static_cast<uint8_t*>(range.first);
    uint8_t* pad_begin = begin + sizeof(Control);
    size_t padding = alignment_padding(pad_begin, k_align_size);
    uint8_t* pool_begin = pad_begin + padding;

    // Mark padding area
#ifdef K_USE_MEM_MARK_PADDING
    std::fill(pad_begin, pool_begin, k_alignment_padding_mark);
#endif

    // Create pool
    create_pool(pool_begin, pool_size);
}

void TLSFAllocator::create_pool(void* pool, std::size_t size)
{
    K_ASSERT(size_t(pool) % k_align_size == 0, "pool memory must be {}B aligned", k_align_size);

    const size_t pool_bytes = align_down(size - k_pool_overhead, k_align_size);
    K_ASSERT(pool_bytes >= k_block_size_min && pool_bytes <= k_block_size_max,
             "bad pool size.\n  -> minimum required: {}\n  -> maximum allowed: {}\n  -> requested: {}",
             k_pool_overhead + k_block_size_min, (k_pool_overhead + k_block_size_max) / 256, pool_bytes);

    /*
        Create the main free block. Offset the start of the block slightly
        so that the prev_phys_block field falls outside of the pool,
        it will never be used.
    */
    BlockHeader* block = BlockHeader::offset_to_block(pool, -std::ptrdiff_t(BlockHeader::k_block_header_overhead));
    block->set_size(pool_bytes);
    block->set_free();
    block->set_prev_used();
    control_->insert_block(block);

    // Split the block to create a zero-size sentinel block
    // The is_last() method will return true on this block
    BlockHeader* next = block->link_next();
    next->set_size(0);
    next->set_used();
    next->set_prev_free();

    pool_ = pool;
}

void TLSFAllocator::walk_pool(PoolWalker walk) const
{
    BlockHeader* block = BlockHeader::offset_to_block(pool_, -std::ptrdiff_t(BlockHeader::k_block_header_overhead));

    while (block && !block->is_last())
    {
        walk(block->to_void_ptr(), block->block_size(), !block->is_free());
        block = block->get_next();
    }
}

TLSFAllocator::IntegrityReport TLSFAllocator::check_pool() const
{
    IntegrityReport report;
    struct
    {
        int prev_status{0};
        int status{0};
    } integrity;

    auto integrity_walker = [&report, &integrity](void* ptr, size_t size, bool used) {
        BlockHeader* block = BlockHeader::from_void_ptr(ptr);
        const int this_prev_status = block->is_prev_free() ? 1 : 0;
        const int this_status = block->is_free() ? 1 : 0;
        int status = 0;
        (void)used;

        if (integrity.prev_status != this_prev_status)
        {
            --status;
            report.logs.push_back(fmt::format("prev status incorrect, at {:016x}", size_t(ptr)));
        }
        if (size != block->block_size())
        {
            --status;
            report.logs.push_back(
                fmt::format("block size incorrect, at {:016x}, {} vs {}", size_t(ptr), size, block->block_size()));
        }

        integrity.prev_status = this_status;
        integrity.status += status;
    };

    walk_pool(integrity_walker);

    return report;
}

TLSFAllocator::IntegrityReport TLSFAllocator::check_consistency() const
{
    IntegrityReport report;

    // Check that the free lists and bitmaps are accurate
    for (int32_t ii = 0; ii < int32_t(k_fl_index_count); ++ii)
    {
        for (int32_t jj = 0; jj < int32_t(k_sl_index_count); ++jj)
        {
            const int32_t fl_map = control_->fl_bitmap & (1 << ii);
            const int32_t sl_list = control_->sl_bitmap[ii];
            const int32_t sl_map = sl_list & (1 << jj);

            BlockHeader* block = control_->blocks[ii][jj];

            // Check that first- and second-level lists agree
            if (!fl_map && sl_map)
            {
                report.logs.push_back(fmt::format("[{}][{}]: second-level map must be null", ii, jj));
            }
            if (!sl_map)
            {
                if (block != &control_->null_block)
                {
                    report.logs.push_back(fmt::format("[{}][{}]: block list must be null", ii, jj));
                }

                continue;
            }

            // Check that there is at least one free block
            if (!sl_list)
            {
                report.logs.push_back(fmt::format("[{}][{}]: no free blocks in second-level map", ii, jj));
            }
            if (block == &control_->null_block)
            {
                report.logs.push_back(fmt::format("[{}][{}]: block should not be null", ii, jj));
            }

            while (block != &control_->null_block)
            {
                if (!block->is_free())
                {
                    report.logs.push_back(
                        fmt::format("[{}][{}] @{:016x} : block should be free", ii, jj, size_t(block)));
                }
                if (block->is_prev_free())
                {
                    report.logs.push_back(
                        fmt::format("[{}][{}] @{:016x} : blocks should have coalesced", ii, jj, size_t(block)));
                }
                if (block->get_next()->is_free())
                {
                    report.logs.push_back(fmt::format("[{}][{}] @{:016x} : blocks should have coalesced", ii, jj,
                                                      size_t(block->get_next())));
                }
                if (!block->get_next()->is_prev_free())
                {
                    report.logs.push_back(
                        fmt::format("[{}][{}] @{:016x} : block should be free", ii, jj, size_t(block->get_next())));
                }
                if (block->block_size() < k_block_size_min)
                {
                    report.logs.push_back(fmt::format("[{}][{}] @{:016x} : block is too small", ii, jj, size_t(block)));
                }

                int fli, sli;
                mapping_insert(block->block_size(), fli, sli);

                if (fli != ii || sli != jj)
                {
                    report.logs.push_back(
                        fmt::format("[{}][{}] @{:016x} block size indexed in wrong list (fli={}, sli={})", ii, jj,
                                    size_t(block), fli, sli));
                }

                block = block->next_free;
            }
        }
    }

    return report;
}

void* TLSFAllocator::allocate(std::size_t size, std::size_t alignment, std::size_t user_offset)
{
    // No need to worry about alignment smaller than k_align_size (allocations abide by a stricter alignment constraint)

    // TMP: disallow higher alignment for now
    (void)user_offset;
    (void)alignment;
    K_ASSERT(alignment <= k_align_size, "higher custom alignment is not implemented yet");

    // Custom higher alignment is handled by a special function
    // if (alignment > k_align_size)
    //     return allocate_aligned(size, alignment, user_offset);

    // Adjust size for alignment and prepare block
    const size_t adjust = adjust_request_size(size, k_align_size);
    BlockHeader* block = control_->locate_free_block(adjust);
    return control_->prepare_used(block, adjust);
}

void* TLSFAllocator::allocate_aligned(std::size_t size, std::size_t alignment, std::size_t user_offset)
{
    // NOTE(ndx): original implementation tries to align the block, but we
    // want to align the user pointer instead.
    // TODO(ndx): modify this function to align the user pointer.
    (void)user_offset;

    const size_t adjust = adjust_request_size(size, k_align_size);

    /*
        We must allocate an additional minimum block size bytes so that if
        our free block will leave an alignment gap which is smaller, we can
        trim a leading free block and release it back to the pool. We must
        do this because the previous physical block is in use, therefore
        the prev_phys_block field is not valid, and we can't simply adjust
        the size of that block.
    */
    const size_t min_gap = sizeof(BlockHeader);
    const size_t size_with_gap = adjust_request_size(adjust + alignment + min_gap, alignment);

    /*
        If alignment is less than or equals base alignment, we're done.
        If we requested 0 bytes, return null, as allocate(0) does.
    */
    const size_t aligned_size = (adjust && alignment > k_align_size) ? size_with_gap : adjust;
    BlockHeader* block = control_->locate_free_block(aligned_size);

    if (block)
    {
        void* ptr = block->to_void_ptr();
        void* aligned = align_ptr(ptr, alignment);
        size_t gap = size_t(std::ptrdiff_t(aligned) - std::ptrdiff_t(ptr));

        // If gap size is too small, offset to next aligned boundary
        if (gap && gap < min_gap)
        {
            const size_t gap_remain = min_gap - gap;
            const size_t offset = std::max(gap_remain, alignment);
            const void* next_aligned = reinterpret_cast<void*>(std::ptrdiff_t(aligned) + std::ptrdiff_t(offset));

            aligned = align_ptr(next_aligned, alignment);
            gap = size_t(std::ptrdiff_t(aligned) - std::ptrdiff_t(ptr));
        }

        if (gap)
        {
            K_ASSERT(gap >= min_gap, "gap size is too small: Minimum: {}, got: {}", min_gap, gap);
            block = control_->trim_free_leading(block, gap);
        }
    }

    return control_->prepare_used(block, adjust);
}

void* TLSFAllocator::reallocate(void* ptr, std::size_t size, std::size_t alignment, std::size_t offset)
{
    // Zero size means free
    if (size == 0 && ptr == nullptr)
    {
        deallocate(ptr);
        return nullptr;
    }
    // Requests with null pointers are treated as allocations
    else if (ptr == nullptr)
    {
        return allocate(size, alignment, offset);
    }
    else
    {
        BlockHeader* block = BlockHeader::from_void_ptr(ptr);
        K_ASSERT(!block->is_free(), "block already marked as free");

        BlockHeader* next = block->get_next();
        const size_t cursize = block->block_size();
        const size_t combined = cursize + next->block_size() + BlockHeader::k_block_header_overhead;
        const size_t adjust = adjust_request_size(size, k_align_size);

        // If next block is used or not large enough, we must reallocate and copy data
        if (adjust > cursize && (!next->is_free() || adjust > combined))
        {
            void* newptr = allocate(size, alignment, offset);
            if (newptr)
            {
                std::memcpy(newptr, ptr, std::min(cursize, size));
                deallocate(ptr);
            }
            return newptr;
        }
        else
        {
            // Do we need to expand to the next block?
            if (adjust > cursize)
            {
                control_->merge_next(block);
                block->mark_as_used();
            }

            // Trim resulting block and return original pointer
            control_->trim_used(block, adjust);
            return ptr;
        }
    }
}

void TLSFAllocator::deallocate(void* ptr)
{
    if (ptr)
    {
        BlockHeader* block = BlockHeader::from_void_ptr(ptr);
        K_ASSERT(!block->is_free(), "block already marked as free");

        block->mark_as_free();
        block = control_->merge_prev(block);
        block = control_->merge_next(block);
        control_->insert_block(block);
    }
}

} // namespace kb::memory