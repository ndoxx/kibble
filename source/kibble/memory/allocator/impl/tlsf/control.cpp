#include "memory/allocator/impl/tlsf/control.h"

namespace kb::memory::tlsf
{

void Control::construct()
{
    null_block.next_free = &null_block;
    null_block.prev_free = &null_block;

    fl_bitmap = 0;
    for (size_t ii = 0; ii < k_fl_index_count; ++ii)
    {
        sl_bitmap[ii] = 0;
        for (size_t jj = 0; jj < k_sl_index_count; ++jj)
        {
            blocks[ii][jj] = &null_block;
        }
    }
}

BlockHeader* Control::search_suitable_block(int32_t& fli, int32_t& sli)
{
    // First, search for a block in the list associated with the given indices
    int32_t sl_map = sl_bitmap[fli] & (~0 << sli);
    if (!sl_map)
    {
        // No block exists. Search in the next largest first-level list
        const int32_t fl_map = fl_bitmap & (~0 << (fli + 1));
        // No free blocks available, out of memory
        if (!fl_map)
            return nullptr;

        fli = ffs(fl_map);
        sl_map = sl_bitmap[fli];
    }

    K_ASSERT(sl_map, "internal error - second level bitmap is null", nullptr);
    sli = ffs(sl_map);
    return blocks[fli][sli];
}

void Control::remove_free_block(BlockHeader* block, int32_t fli, int32_t sli)
{
    BlockHeader* prev = block->prev_free;
    BlockHeader* next = block->next_free;
    K_ASSERT(prev, "prev_free field can not be null", nullptr);
    K_ASSERT(next, "next_free field can not be null", nullptr);
    next->prev_free = prev;
    prev->next_free = next;

    // If this block is the head of the free list, set new head
    if (blocks[fli][sli] == block)
    {
        blocks[fli][sli] = next;

        // If the new head is null, clear the bitmap
        if (next == &null_block)
        {
            sl_bitmap[fli] &= ~(1 << sli);

            // If the second bitmap is now empty, clear the fl bitmap
            if (!sl_bitmap[fli])
                fl_bitmap &= ~(1 << fli);
        }
    }
}

void Control::insert_free_block(BlockHeader* block, int32_t fli, int32_t sli)
{
    BlockHeader* current = blocks[fli][sli];
    K_ASSERT(current, "free list cannot have a null entry", nullptr);
    K_ASSERT(block, "cannot insert a null entry into the free list", nullptr);
    block->next_free = current;
    block->prev_free = &null_block;
    current->prev_free = block;

    K_ASSERT(block->to_void_ptr() == align_ptr(block->to_void_ptr(), k_align_size), "block not aligned properly",
             nullptr);
    /*
    ** Insert the new block at the head of the list, and mark the first-
    ** and second-level bitmaps appropriately.
    */
    blocks[fli][sli] = block;
    fl_bitmap |= (1 << fli);
    sl_bitmap[fli] |= (1 << sli);
}

void Control::remove_block(BlockHeader* block)
{
    int32_t fli, sli;
    mapping_insert(block->block_size(), fli, sli);
    remove_free_block(block, fli, sli);
}

void Control::insert_block(BlockHeader* block)
{
    int32_t fli, sli;
    mapping_insert(block->block_size(), fli, sli);
    insert_free_block(block, fli, sli);
}

/**
 * @internal
 * @brief Absorb a free block's storage into an adjacent previous free block
 *
 * @param prev
 * @param block
 * @return BlockHeader*
 */
BlockHeader* absorb(BlockHeader* prev, BlockHeader* block)
{
    K_ASSERT(!prev->is_last(), "previous block can't be last", nullptr);
    // NOTE: Leaves flags untouched
    prev->size += block->block_size() + BlockHeader::k_block_header_overhead;
    prev->link_next();
    return prev;
}

BlockHeader* Control::merge_prev(BlockHeader* block)
{
    if (block->is_prev_free())
    {
        BlockHeader* prev = block->get_prev();
        K_ASSERT(prev, "prev physical block can't be null", nullptr);
        K_ASSERT(prev->is_free(), "prev block is not free though marked as such", nullptr);
        remove_block(prev);
        block = absorb(prev, block);
    }

    return block;
}

BlockHeader* Control::merge_next(BlockHeader* block)
{
    BlockHeader* next = block->get_next();
    K_ASSERT(next, "next physical block can't be null", nullptr);

    if (next->is_free())
    {
        K_ASSERT(!block->is_last(), "previous block can't be last", nullptr);
        remove_block(next);
        block = absorb(block, next);
    }

    return block;
}

void Control::trim_free(BlockHeader* block, size_t size)
{
    K_ASSERT(block->is_free(), "block must be free", nullptr);
    if (block->can_split(size))
    {
        BlockHeader* remaining_block = block->split(size);
        block->link_next();
        remaining_block->set_prev_free();
        insert_block(remaining_block);
    }
}

BlockHeader* Control::trim_free_leading(BlockHeader* block, size_t size)
{
    BlockHeader* remaining_block = block;
    if (block->can_split(size))
    {
        // We want the 2nd block
        remaining_block = block->split(size - BlockHeader::k_block_header_overhead);
        remaining_block->set_prev_free();

        block->link_next();
        insert_block(block);
    }

    return remaining_block;
}

void Control::trim_used(BlockHeader* block, size_t size)
{
    K_ASSERT(!block->is_free(), "block must be used", nullptr);
    if (block->can_split(size))
    {
        // If the next block is free, we must coalesce
        BlockHeader* remaining_block = block->split(size);
        remaining_block->set_prev_used();
        remaining_block = merge_next(remaining_block);
        insert_block(remaining_block);
    }
}

BlockHeader* Control::locate_free_block(size_t size)
{
    int32_t fli = 0, sli = 0;
    BlockHeader* block = nullptr;

    if (size)
    {
        mapping_search(size, fli, sli);

        /*
            mapping_search can futz with the size, so for excessively large sizes it can sometimes wind up
            with indices that are off the end of the block array.
            So, we protect against that here, since this is the only callsite of mapping_search.
            Note that we don't need to check sli, since it comes from a modulo operation that guarantees
            it's always in range.
        */
        if (fli < int32_t(k_fl_index_count))
            block = search_suitable_block(fli, sli);
    }

    if (block)
    {
        K_ASSERT(block->block_size() >= size, "could not locate free block large enough", nullptr)
            .watch_var__(size, "requested")
            .watch_var__(block->block_size(), "available");

        remove_free_block(block, fli, sli);
    }

    return block;
}

void* Control::prepare_used(BlockHeader* block, size_t size)
{
    if (block)
    {
        K_ASSERT(size, "size must be non-zero", nullptr);
        trim_free(block, size);
        block->mark_as_used();
        return block->to_void_ptr();
    }
    return nullptr;
}

} // namespace kb::memory::tlsf