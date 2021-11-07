#pragma once

#include <cstdint>

namespace kb
{
namespace memory
{

#pragma pack(push)
#pragma pack(1) // Force 1-byte alignment to avoid undefined behavior during (UGLY) union-cast in init()
/**
 * @brief Implementation of a free list data structure.
 * Used in the PoolAllocator class.
 *
 */
class Freelist
{
public:
    Freelist() = default;

    /**
     * @brief Construct a free list.
     *
     * @param begin pointer to the start of the allocated memory block
     * @param element_size size of each node in the free list
     * @param max_elements maximum number of elements
     * @param alignment alignment requirement (unused)
     * @param offset offset requirement (unused)
     */
    Freelist(void *begin, std::size_t element_size, std::size_t max_elements, std::size_t alignment,
             std::size_t offset);

    /**
     * @brief Initialize a free list.
     *
     * @param begin pointer to the start of the allocated memory block
     * @param element_size size of each node in the free list
     * @param max_elements maximum number of elements
     * @param alignment alignment requirement (unused)
     * @param offset offset requirement (unused)
     */
    void init(void *begin, std::size_t element_size, std::size_t max_elements, std::size_t alignment,
              std::size_t offset);

    /**
     * @brief Get a pointer to the next unallocated block.
     *
     * @return pointer to the newt available block or nullptr if there is no more room
     */
    inline void *acquire()
    {
        // Return null if no more entry left
        if (next_ == nullptr)
            return nullptr;

        // Obtain one element from the head of the free list
        Freelist *head = next_;
        next_ = head->next_;
        return head;
    }

	/**
	 * @brief Return a block to the free list.
	 * 
	 * @param ptr 
	 */
    inline void release(void *ptr)
    {
        // Put the returned element at the head of the free list
        Freelist *head = static_cast<Freelist *>(ptr);
        head->next_ = next_;
        next_ = head;
    }

#ifdef K_DEBUG
    inline void *next(void *ptr)
    {
        return static_cast<Freelist *>(ptr)->next_;
    }
#endif

private:
    Freelist *next_;
};
#pragma pack(pop)

} // namespace memory
} // namespace kb