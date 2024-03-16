#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace kb::memory
{

namespace tlsf
{
struct Control;
}

class HeapArea;

/**
 * @brief Two-Level Segregated Fit memory allocator.
 *
 * Nodes of any size can be allocated / deallocated in constant time.
 * For the theory, see:
 * https://www.researchgate.net/publication/4080369_TLSF_A_new_dynamic_memory_allocator_for_real-time_systems
 *
 * This is an experimental port of the C implementation by Matthew Conte, see:
 * https://github.com/mattconte/tlsf
 *
 * Original code is released under BSD license.
 * Changes are:
 *      - Naming conventions
 *      - A few simplifications allowed by C++
 *      - The implementation is split into multiple files
 *      - The interface fits my memory arena requirements
 *      - Only one pool allowed
 *
 * @todo This is still experimental and incomplete:
 *      - Implement correct alignment strategy
 *      - Implement reallocate
 *
 */
class TLSFAllocator
{
public:
    /**
     * @brief Reserve a block on a HeapArea and use it for TLSF allocation.
     *
     * @param area reference to the memory resource the allocator will reserve a block from
     * @param debug_name name of this allocator, for debug purposes
     * @param pool_size size of the allocation pool
     */
    TLSFAllocator(const char* debug_name, HeapArea& area, uint32_t decoration_size, std::size_t pool_size);

    /**
     * @brief Allocate an 8B-aligned block of memory of a given size.
     *
     * @param size Minimum size to allocate
     * @param alignment UNUSED FOR NOW
     * @param offset UNUSED FOR NOW
     * @return void* Pointer to the beginning of the block
     */
    void* allocate(std::size_t size, std::size_t alignment, std::size_t offset);

    /**
     * @brief Try to extend memory block in place if possible, reallocate a bigger block elsewhere and copy data if not
     * possible.
     *
     * @note Passing 0 in the size argument is equivalent to calling deallocate()
     * @note Passing a null pointer is equivalent to calling allocate()
     *
     * @todo IMPLEMENT THIS
     *
     * @param ptr
     * @param size
     * @param alignment
     * @param offset
     * @return void*
     */
    void* reallocate(void* ptr, std::size_t size, std::size_t alignment, std::size_t offset);

    /**
     * @brief Free memory block and coalesce immediately
     *
     * @param ptr
     */
    void deallocate(void* ptr);

    // * Debug
    /// @brief Use this type of functor to visit each block in the pool
    using PoolWalker = std::function<void(void*, size_t, bool)>;

    /// @brief Gathers all integrity errors strings produced by the checker functions
    struct IntegrityReport
    {
        std::vector<std::string> logs;
    };

    /**
     * @brief Visit each block in the pool
     *
     * @param walk Visitor object
     */
    void walk_pool(PoolWalker walk) const;

    /**
     * @brief Check pool integrity
     *
     * @return IntegrityReport a report containing all errors encountered
     */
    IntegrityReport check_pool() const;

    /**
     * @brief Check control structure integrity
     *
     * @return IntegrityReport a report containing all errors encountered
     */
    IntegrityReport check_consistency() const;

private:
    /**
     * @internal
     * @brief Helper function to initialize a pool after the control structure
     *
     * @param begin
     * @param size
     */
    void create_pool(void* begin, std::size_t size);

    /**
     * @internal
     * @brief Basically a memalign() implementation
     *
     * This is called by allocate() when required alignment is greater than base alignment.
     *
     * @todo Take offset into account
     *
     * @param size
     * @param alignment
     * @param offset
     * @return void*
     */
    void* allocate_aligned(std::size_t size, std::size_t alignment, std::size_t offset);

private:
    tlsf::Control* control_{nullptr};
    void* pool_{nullptr};
};

} // namespace kb::memory