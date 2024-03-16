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

    void* allocate(std::size_t size, std::size_t alignment, std::size_t offset);
    void* reallocate(void* ptr, std::size_t size, std::size_t alignment, std::size_t offset);
    void deallocate(void* ptr);

    void reset();

    // * Debug
    using PoolWalker = std::function<void(void*, size_t, bool)>;

    struct IntegrityReport
    {
        std::vector<std::string> logs;
    };

    void walk_pool(PoolWalker walk) const;
    IntegrityReport check_pool() const;
    IntegrityReport check_consistency() const;

private:
    void create_pool(void* begin, std::size_t size);

private:
    tlsf::Control* control_{nullptr};
    void* pool_{nullptr};
    uint32_t decoration_size_;
};

} // namespace kb::memory