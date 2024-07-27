#pragma once

#include "memory/allocator/tlsf/impl/common.h"

namespace kb::memory::tlsf
{

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
    static constexpr size_t k_block_start_offset = sizeof(BlockHeader*) + sizeof(size);

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
    static inline BlockHeader* offset_to_block(void* ptr, std::ptrdiff_t offset)
    {
        return reinterpret_cast<BlockHeader*>(static_cast<unsigned char*>(ptr) + offset);
    }

    /**
     * @internal
     * @brief Return location of previous block
     *
     * @return BlockHeader*
     */
    inline BlockHeader* get_prev() const
    {
        K_ASSERT(is_prev_free(), "Previous block must be free");
        return prev_physical;
    }

    /**
     * @internal
     * @brief Return location of next existing block
     *
     * @return BlockHeader*
     */
    inline BlockHeader* get_next()
    {
        K_ASSERT(!is_last(), "Next block must be busy");
        return offset_to_block(to_void_ptr(), std::ptrdiff_t(block_size() - k_block_header_overhead));
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

static_assert(sizeof(BlockHeader) == k_block_size_min + BlockHeader::k_block_header_overhead,
              "invalid block header size");

} // namespace kb::memory::tlsf