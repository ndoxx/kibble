#pragma once

#include <cstdint>
#include <ostream>
#include <utility>

#include "free_list.h"

namespace kb
{
namespace memory
{

class HeapArea;

/**
 * @brief Used to allocate nodes of a fixed size.
 * Nodes can be allocated and deallocated in constant time, with no overhead due to the fact that the underlying memory
 * resource is already reserved on the heap. This makes this allocator quite useful when a huge number of small objects
 * need to be allocated and deallocated rapidly, like in a particle system for example.
 * In theory, objects of different sizes can be used in a memory pool, however the node element needs to be equal to the
 * size of the largest object. So in this configuration, some memory is just wasted, plain and simple.
 *
 * This allocator uses a FreeList data structure at its core to easily locate the next available chunk of memory.
 *
 */
class PoolAllocator
{
public:
    /**
     * @brief Reserve a block of a given size on a HeapArea and use it for pool allocation.
     *
     * @param area reference to the memory resource the allocator will reserve a block from
     * @param node_size size of a node
     * @param max_nodes maximum amount of nodes in the memory pool
     * @param debug_name name of this allocator, for debug purposes
     */
    PoolAllocator(const char* debug_name, HeapArea& area, uint32_t decoration_size, std::size_t node_size, std::size_t max_nodes);

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
     * If the symbol ALLOCATOR_PADDING_MAGIC is defined, padded zones will be memset to a fixed magic number.
     *
     * @param size size of the chunk to allocate
     * @param alignment alignment constraint, such that `(returned_pointer + offset) % alignment == 0`
     * @param offset offset to the user pointer
     * @return the returned pointer, at the beginning of the chunk, or nullptr if the allocator reached the end of the
     * block
     */
    void* allocate(std::size_t size, std::size_t alignment = 0, std::size_t offset = 0);

    /**
     * @brief Deallocate a node.
     * The node will be returned to the free list and will be available for future allocations.
     *
     * @param ptr
     */
    void deallocate(void* ptr);

    /**
     * @brief This function is left empty because mass deallocating everything would be dangerous.
     *
     */
    inline void reset()
    {
    }

private:
    std::size_t node_size_;
    std::size_t max_nodes_;
    uint8_t* begin_;
    uint8_t* end_;
    Freelist free_list_;
};

} // namespace memory
} // namespace kb