#pragma once

#include "../../assert/assert.h"
#include "../../math/constexpr_math.h"
#include "../../util/sanitizer.h"
#include "../heap_area.h"
#include "../util/alignment.h"
#include "config.h"

#include "atomic_queue/atomic_queue.h"

namespace kb
{
namespace memory
{

/**
 * @brief Pool allocator with atomic access.
 * It works similarly to PoolAllocator, however, instead of a FreeList, it uses
 * an atomic queue to store addresses to all free nodes.
 *
 * @note The reason why the PoolAllocator's FreeList was not turned into a policy
 * is because the atomic queue used here forces the following class to have
 * a compile-time MAX_NODES parameters, which is incompatible with PoolAllocator's
 * API. As a result, most of the code here is just a copy of PoolAllocator code.
 *
 * @todo A proper implementation could be made by copying the relevant code from
 * atomic_queue, and tailoring it for this allocator, allowing for a runtime MAX_NODES.
 *
 * @param max_nodes maximum amount of nodes in the memory pool
 */
template <size_t MAX_NODES>
class AtomicPoolAllocator
{
public:
    /**
     * @brief Reserve a block of a given size on a HeapArea and use it for pool allocation.
     *
     * @param area reference to the memory resource the allocator will reserve a block from
     * @param decoration_size size of additional data at the beginning of each node
     * @param user_size maximum size of object allocated by the user
     * @param max_alignment maximum alignment requirement
     * @param debug_name name of this allocator, for debug purposes
     */
    AtomicPoolAllocator(const char* debug_name, HeapArea& area, uint32_t decoration_size, std::size_t user_size,
                        std::size_t max_alignment)
    {
        node_size_ = math::round_up_pow2(int32_t(user_size + decoration_size), int32_t(max_alignment));
        auto range = area.require_block(node_size_ * MAX_NODES, debug_name);
        begin_ = static_cast<uint8_t*>(range.first);
        end_ = begin_ + MAX_NODES * node_size_;

        // Fill the free queue with all possible addresses
        for (uint8_t* runner = begin_; runner < end_; runner += node_size_)
        {
            free_queue_.push(runner);
        }
    }

    /**
     * @brief Return a pointer to the beginning of the block.
     *
     * @return uint8_t*
     */
    inline uint8_t* begin()
    {
        return begin_;
    }

    /**
     * @brief Return a const pointer to the beginning of the block.
     *
     * @return const uint8_t*
     */
    inline const uint8_t* begin() const
    {
        return begin_;
    }

    /**
     * @brief Return a pointer to the end of the block.
     *
     * @return uint8_t*
     */
    inline uint8_t* end()
    {
        return end_;
    }

    /**
     * @brief Return a const pointer to the end of the block.
     *
     * @return uint8_t*
     */
    inline const uint8_t* end() const
    {
        return end_;
    }

    /**
     * @brief Allocate a chunk of a given size at the next available position.
     * This function supports alignment constraints. As allocators are used indirectly through a memory arena, the
     * pointer passed to the user in the end (called the user pointer) may need to be offset with respect to the pointer
     * returned by this function. This may happen when the memory arena needs to put a sentinel before the user data
     * when the sanitizer is active, or in the case of an array allocation where the size of the array is put before the
     * array pointer. This function will align the user pointer (returned_pointer + offset).
     *
     * The supplied size argument should not exceed the node size minus the padding due to the alignment:\n
     * `size + padding <= node_size`\n
     * this is enforced by a K_ASSERT
     *
     * If the symbol K_USE_MEM_MARK_PADDING is defined, padded zones will be memset to a fixed magic number.
     *
     * @param size size of the chunk to allocate
     * @param alignment alignment constraint, such that `(returned_pointer + offset) % alignment == 0`
     * @param offset offset to the user pointer
     * @return the returned pointer, at the beginning of the chunk, or nullptr if the allocator reached the end of the
     * block
     */
    void* allocate([[maybe_unused]] std::size_t size, std::size_t alignment = 0, std::size_t offset = 0)
    {
        ANNOTATE_HAPPENS_AFTER(&free_queue_); // Avoid false positives with TSan
        uint8_t* next = free_queue_.pop();

        // We want the user pointer (at next+offset) to be aligned.
        // Check if alignment is required. If so, find the next aligned memory address.
        std::size_t padding = alignment_padding(next + offset, alignment);

        K_ASSERT(padding + size <= node_size_,
                 "[AtomicPoolAllocator] Allocation size does not fit initial requirement.\n  -> requested size: {}\n  "
                 "-> node size: {}\n  -> data size: {}\n  -> offset: {}\n  -> alignment: {}\n  -> padding: {}",
                 padding + size, node_size_, size, offset, alignment, padding);

        // Mark padding area
#ifdef K_USE_MEM_MARK_PADDING
        std::fill(next, next + padding, k_alignment_padding_mark);
#endif

        return next + padding;
    }

    /**
     * @brief Deallocate a node.
     * The node will be returned to the free list and will be available for future allocations.
     *
     * @param ptr
     */
    void deallocate(void* ptr)
    {
        // Get unaligned address
        size_t offset = size_t(static_cast<uint8_t*>(ptr) - begin_); // Distance in bytes to beginning of the block
        size_t padding = offset % node_size_; // Distance in bytes to beginning of the node = padding

        ANNOTATE_HAPPENS_BEFORE(&free_queue_); // Avoid false positives with TSan
        free_queue_.push(static_cast<uint8_t*>(ptr) - padding);
    }

    /**
     * @brief This function is left empty because mass deallocating everything would be dangerous.
     *
     */
    inline void reset()
    {
    }

private:
    std::size_t node_size_;
    uint8_t* begin_;
    uint8_t* end_;
    atomic_queue::AtomicQueue<uint8_t*, MAX_NODES, nullptr, true, true, false, false> free_queue_;
};

} // namespace memory
} // namespace kb