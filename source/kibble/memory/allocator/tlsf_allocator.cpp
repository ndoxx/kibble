#include "memory/allocator/tlsf_allocator.h"
#include "assert/assert.h"
#include "memory/allocator/impl/tlsf_bit.h"

#include <cstddef>

using std::size_t;

namespace kb::memory
{

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

struct BlockHeader
{
    // * Data
    // Points to the previous physical block
    BlockHeader* prev_physical;
    // Size of this block, excluding header
    size_t size;
    // Next / previous free blocks (unused / overwritten if block is allocated)
    BlockHeader* next_free;
    BlockHeader* prev_free;

    // * Constants
    /*
        Block sizes are always at least a multiple of 4 (minimum allocation size is 4B),
        so we're free to use the two last bits of the size field to store block status:
        - bit 0: whether the block is free / busy (used)
        - bit 1: whether the previous block is free / busy
    */
    static constexpr size_t k_block_header_free_bit = 1ull << 0;
    static constexpr size_t k_block_header_prev_free_bit = 1ull << 1;
    /*
        The prev_physical field is stored *inside* the previous free block,
        and the last two intrusive pointers are overwritten by block data when block is busy.
        So the size of the block header as exposed to used blocks is just the size of the size field.
    */
    static constexpr size_t k_block_header_overhead = sizeof(size_t);

    /*
        User data starts directly after the size field in a busy block.
        Cannot use offsetof here (not constexpr), ASSUME the two fields are packed.
        It should be the case, a pointer to a struct is 8B, and the size member is
        a size_t which is 8B large and 8B aligned.
    */
    static constexpr size_t k_block_start_offset = sizeof(prev_physical) + sizeof(size);

    // * Functionality

    /**
     * @internal
     * @brief Get block size
     *
     * @return size_t
     */
    inline size_t block_size() const
    {
        // Remove the last two flag bits
        return size & ~(k_block_header_free_bit | k_block_header_prev_free_bit);
    }

    /**
     * @internal
     * @brief Set size of this block, without touching free bits
     *
     * @param size_
     */
    inline void set_size(size_t size_)
    {
        size = size_ | (size & (k_block_header_free_bit | k_block_header_prev_free_bit));
    }

    /**
     * @internal
     * @brief Check if this block has no next neighbor
     *
     * @return true
     * @return false
     */
    inline bool is_last() const
    {
        return block_size() == 0;
    }

    /**
     * @internal
     * @brief Check if this block is free
     *
     * @return true
     * @return false
     */
    inline bool is_free() const
    {
        return bool(size & k_block_header_free_bit);
    }

    /**
     * @internal
     * @brief Set free bit for this block
     *
     */
    inline void set_free()
    {
        size |= k_block_header_free_bit;
    }

    /**
     * @internal
     * @brief Clear free bit for this block
     *
     */
    inline void set_used()
    {
        size &= ~k_block_header_free_bit;
    }

    /**
     * @internal
     * @brief Check if previous block is free
     *
     * @return true
     * @return false
     */
    inline bool is_prev_free() const
    {
        return bool(size & k_block_header_prev_free_bit);
    }

    /**
     * @internal
     * @brief Set free bit for previous block
     *
     */
    inline void set_prev_free()
    {
        size |= k_block_header_prev_free_bit;
    }

    /**
     * @internal
     * @brief Clear free bit for previous block
     *
     */
    inline void set_prev_used()
    {
        size &= ~k_block_header_prev_free_bit;
    }

    /**
     * @internal
     * @brief Get a const block pointer from a const void pointer
     *
     * @param ptr
     * @return const BlockHeader*
     */
    static inline const BlockHeader* from_void_ptr(const void* ptr)
    {
        return reinterpret_cast<const BlockHeader*>(reinterpret_cast<const unsigned char*>(ptr) - k_block_start_offset);
    }

    /**
     * @internal
     * @brief Get a block pointer from a void pointer
     *
     * @param ptr
     * @return BlockHeader*
     */
    static inline BlockHeader* from_void_ptr(void* ptr)
    {
        return reinterpret_cast<BlockHeader*>(reinterpret_cast<unsigned char*>(ptr) - k_block_start_offset);
    }

    /**
     * @internal
     * @brief Convert this block to a const void pointer
     *
     * @return const void*
     */
    inline const void* to_void_ptr() const
    {
        return reinterpret_cast<const void*>(reinterpret_cast<const unsigned char*>(this) + k_block_start_offset);
    }

    /**
     * @internal
     * @brief Convert this block to a void pointer
     *
     * @return void*
     */
    inline void* to_void_ptr()
    {
        return reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(this) + k_block_start_offset);
    }

    /**
     * @internal
     * @brief Return location of next block after block of given size
     *
     * @param ptr
     * @param size
     * @return BlockHeader*
     */
    static inline BlockHeader* offset_to_block(const void* ptr, size_t size)
    {
        return reinterpret_cast<BlockHeader*>(std::ptrdiff_t(ptr) + std::ptrdiff_t(size));
    }

    /**
     * @internal
     * @brief Return location of previous block
     *
     * @return BlockHeader*
     */
    inline BlockHeader* get_prev() const
    {
        K_ASSERT(is_prev_free(), "Previous block must be free", nullptr);
        return prev_physical;
    }

    /**
     * @internal
     * @brief Return location of next existing block
     *
     * @return BlockHeader*
     */
    inline BlockHeader* get_next() const
    {
        K_ASSERT(!is_last(), "Next block must be busy", nullptr);
        return offset_to_block(to_void_ptr(), block_size() - k_block_header_overhead);
    }

    /**
     * @internal
     * @brief Link a new block with its physical neighbor
     *
     * @return BlockHeader* the neighbor
     */
    inline BlockHeader* link_next()
    {
        BlockHeader* next = get_next();
        next->prev_physical = this;
        return next;
    }

    /**
     * @internal
     * @brief Check if we can split this block
     *
     * @param size
     * @return true
     * @return false
     */
    inline bool can_split(size_t size_request)
    {
        return block_size() >= sizeof(BlockHeader) + size_request;
    }

    /**
     * @internal
     * @brief Link to next block and set this block free
     *
     */
    void mark_as_free();

    /**
     * @internal
     * @brief Set this block as busy, inform next block
     *
     */
    void mark_as_used();

    /**
     * @internal
     * @brief Split a block into two, the second of which is free
     *
     * @param size New block size
     * @return BlockHeader*
     */
    BlockHeader* split(size_t size);
};

/*
    A free block must be large enough to store its header minus the size of the prev_physical
    field, and no larger than the number of addressable bits for first level index.
*/
constexpr size_t k_block_size_min = sizeof(BlockHeader) - sizeof(BlockHeader*);
constexpr size_t k_block_size_max = 1ull << k_fl_index_max;

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
    BlockHeader* remaining = offset_to_block(to_void_ptr(), size_request - k_block_header_overhead);
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
        fli = bit::fls_size_t(size);
        sli = (int(size) >> (fli - int(k_sl_index_count_log2))) ^ int(1 << k_sl_index_count_log2);
        fli -= (k_fl_index_shift - 1);
    }
}

// This version rounds up to the next block size (for allocations)
void mapping_search(size_t size, int& fli, int& sli)
{
    if (size >= k_small_block_size)
    {
        const size_t round = (1 << (size_t(bit::fls_size_t(size)) - k_sl_index_count_log2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

struct TLSFAllocator::Control
{
    // Empty lists point at this block to indicate they are free
    BlockHeader null_block;

    // Bitmaps for free lists
    int32_t fl_bitmap;                   // First Level Bitmap
    int32_t sl_bitmap[k_fl_index_count]; // Second Level Bitmaps

    // Head of free lists
    BlockHeader* blocks[k_fl_index_count][k_sl_index_count];

    /**
     * @brief Clear structure and point all empty lists at the null block
     *
     */
    void construct();

    /**
     * @internal
     * @brief Search next available free block
     *
     * @param fli first level index
     * @param sli second level index
     * @return BlockHeader* found block or nullptr
     */
    BlockHeader* search_suitable_block(int32_t& fli, int32_t& sli);

    /**
     * @internal
     * @brief Remove a free block from the free list
     *
     * @param block The block to remove
     * @param fli first level index
     * @param sli second level index
     */
    void remove_free_block(BlockHeader* block, int32_t fli, int32_t sli);

    /**
     * @internal
     * @brief Insert a free block into the free block list
     *
     * @param block The block to insert
     * @param fli first level index
     * @param sli second level index
     */
    void insert_free_block(BlockHeader* block, int32_t fli, int32_t sli);

    /**
     * @internal
     * @brief Remove a given block from the free list
     *
     * @param block
     */
    inline void remove_block(BlockHeader* block)
    {
        int32_t fli, sli;
        mapping_insert(block->block_size(), fli, sli);
        remove_free_block(block, fli, sli);
    }

    /**
     * @internal
     * @brief Insert a given block into the free list
     *
     * @param block
     */
    inline void insert_block(BlockHeader* block)
    {
        int32_t fli, sli;
        mapping_insert(block->block_size(), fli, sli);
        insert_free_block(block, fli, sli);
    }

    /**
     * @internal
     * @brief Merge a just-freed block with an adjacent previous free block
     *
     * @param block
     * @return BlockHeader*
     */
    BlockHeader* merge_prev(BlockHeader* block);

    /**
     * @internal
     * @brief Merge a just-freed block with an adjacent free block
     *
     * @param block
     * @return BlockHeader*
     */
    BlockHeader* merge_next(BlockHeader* block);

    /**
     * @internal
     * @brief Trim any trailing block space off the end of a block, return to pool
     *
     * @param block
     * @param size
     */
    void trim_free(BlockHeader* block, size_t size);

    /**
     * @internal
     * @brief
     *
     * @param block
     * @param size
     * @return BlockHeader*
     */
    BlockHeader* trim_free_leading(BlockHeader* block, size_t size);

    /**
     * @internal
     * @brief Trim any trailing block space off the end of a used block, return to pool
     *
     * @param block
     * @param size
     */
    void trim_used(BlockHeader* block, size_t size);

    /**
     * @internal
     * @brief Look for the next available free block that is large enough for requested size
     *
     * @param size
     * @return BlockHeader*
     */
    BlockHeader* locate_free_block(size_t size);

    /**
     * @internal
     * @brief Helper to trim free space next to a block and mark it as used
     *
     * @param block
     * @param size
     * @return void* opaque pointer to the block, or nullptr if size is zero
     */
    void* prepare_used(BlockHeader* block, size_t size);
};

void TLSFAllocator::Control::construct()
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

BlockHeader* TLSFAllocator::Control::search_suitable_block(int32_t& fli, int32_t& sli)
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

        fli = bit::ffs(fl_map);
        sl_map = sl_bitmap[fli];
    }

    K_ASSERT(sl_map, "internal error - second level bitmap is null", nullptr);
    sli = bit::ffs(sl_map);
    return blocks[fli][sli];
}

void TLSFAllocator::Control::remove_free_block(BlockHeader* block, int32_t fli, int32_t sli)
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

void TLSFAllocator::Control::insert_free_block(BlockHeader* block, int32_t fli, int32_t sli)
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

BlockHeader* TLSFAllocator::Control::merge_prev(BlockHeader* block)
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

BlockHeader* TLSFAllocator::Control::merge_next(BlockHeader* block)
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

void TLSFAllocator::Control::trim_free(BlockHeader* block, size_t size)
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

BlockHeader* TLSFAllocator::Control::trim_free_leading(BlockHeader* block, size_t size)
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

void TLSFAllocator::Control::trim_used(BlockHeader* block, size_t size)
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

BlockHeader* TLSFAllocator::Control::locate_free_block(size_t size)
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
            Note that we don't need to check sl, since it comes from a modulo operation that guarantees it's always in
            range.
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

void* TLSFAllocator::Control::prepare_used(BlockHeader* block, size_t size)
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

} // namespace kb::memory