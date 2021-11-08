#pragma once

#include <cstdint>
#include <utility>

namespace kb
{
namespace memory
{

class HeapArea;
/**
 * @brief Used to allocate chunks of arbitrary sizes one after the other.
 * This is intended to be used when elements of various sizes need to be allocated very frequently and all deallocated
 * at once (command patterns come to mind). In such a scenario, it is cumbersome to track each and every pointer for
 * future deallocation. A simple call to reset() will move the head back to the beginning of the block and new
 * allocations can happen.
 * The deallocate() function of this allocator is left empty, as there is no good way to do it. Only the reset()
 * function need be called.
 *
 */
class LinearAllocator
{
public:
    LinearAllocator() = default;

    /**
     * @brief Reserve a block of a given size on a HeapArea and use it for linear allocation.
     *
     * @param area reference to the memory resource the allocator will reserve a block from
     * @param size size of the block to reserve
     * @param debug_name name of this allocator, for debug purposes
     */
    LinearAllocator(HeapArea &area, std::size_t size, const char *debug_name);

    /**
     * @brief Lazy-initialize a linear allocator.
     * Reserve a block of a given size on a HeapArea and use it for linear allocation.
     *
     * @param area reference to the memory resource the allocator will reserve a block from
     * @param size size of the block to reserve
     * @param debug_name name of this allocator, for debug purposes
     */
    void init(HeapArea &area, std::size_t size, const char *debug_name);

    /**
     * @brief Return a pointer to the beginning of the block.
     *
     * @return uint8_t*
     */
    inline uint8_t *begin()
    {
        return begin_;
    }

    /**
     * @brief Return a const pointer to the beginning of the block.
     *
     * @return const uint8_t*
     */
    inline const uint8_t *begin() const
    {
        return begin_;
    }

    /**
     * @brief Return a pointer to the end of the block.
     *
     * @return uint8_t*
     */
    inline uint8_t *end()
    {
        return end_;
    }

    /**
     * @brief Return a const pointer to the end of the block.
     *
     * @return uint8_t*
     */
    inline const uint8_t *end() const
    {
        return end_;
    }

    /**
     * @brief Allocate a chunk of a given size next to the last one.
     * This function supports alignment constraints. As allocators are used indirectly through a memory arena, the
     * pointer passed to the user in the end (called the user pointer) may need to be offset with respect to the pointer
     * returned by this function. This may happen when the memory arena needs to put a sentinel before the user data
     * when the sanitizer is active, or in the case of an array allocation where the size of the array is put before the
     * array pointer. This function will align the user pointer (returned_pointer + offset).
     *
     * If the symbol ALLOCATOR_PADDING_MAGIC is defined, padded zones will be memset to a fixed magic number.
     *
     * @param size size of the chunk to allocate
     * @param alignment alignment constraint, such that `(returned_pointer + offset) % alignment == 0`
     * @param offset offset to the user pointer
     * @return the returned pointer, at the beginning of the chunk, or nullptr if the allocator reached the end of the
     * block
     */
    void *allocate(std::size_t size, std::size_t alignment, std::size_t offset);

    /**
     * @brief This function is left empty because there is no good way to deallocate stuff with a linear allocator. Only
     * reset() should be called to deallocate everything at once. However, as the allocator is used as a policy in the
     * memory arena, this function needs to exist.
     *
     */
    inline void deallocate(void *)
    {
    }

    /**
     * @brief Moves the head back to the beginning of the block, effectively deallocating everything.
     *
     */
    inline void reset()
    {
        current_offset_ = 0;
    }

private:
    uint8_t *begin_;
    uint8_t *end_;
    uint32_t current_offset_;
};

} // namespace memory
} // namespace kb