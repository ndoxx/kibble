#pragma once

#include "memory/allocator/impl/tlsf/block.h"

namespace kb::memory::tlsf
{

struct Control
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
    Control();

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
    void remove_block(BlockHeader* block);

    /**
     * @internal
     * @brief Insert a given block into the free list
     *
     * @param block
     */
    void insert_block(BlockHeader* block);

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

} // namespace kb::memory::tlsf