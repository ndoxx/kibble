#pragma once

#include <cstdint>

namespace kb::memory
{

class HeapArea;
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

    ~TLSFAllocator();

    void* allocate(std::size_t size, std::size_t alignment, std::size_t offset);
    void* reallocate(void* ptr, std::size_t size, std::size_t alignment, std::size_t offset);
    void deallocate(void* ptr);

    void reset();

private:
    struct Control;

    Control* control_{nullptr};
};

} // namespace kb::memory