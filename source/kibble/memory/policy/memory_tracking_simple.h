#pragma once

#include <cstdint>
#include <string>

namespace kb::memory
{
class HeapArea;
}

namespace kb::log
{
class Channel;
}

namespace kb::memory::policy
{

/**
 * @brief Memory tracking policy that counts (de)allocations.
 * Each time an allocation occurs, a counter is incremented, and each time a deallocation occurs, the counter is
 * decremented. When the program exits, if the counter is not zero, this means the number of allocations does not match
 * the number of deallocations, which indicates a leak.
 *
 */
class SimpleMemoryTracking
{
public:
    void init(const std::string& debug_name, const HeapArea& area);

    /**
     * @brief On allocating a chunk, increase internal counter
     *
     */
    inline void on_allocation(uint8_t*, std::size_t, std::size_t, const char*, int)
    {
        ++num_allocs_;
    }

    /**
     * @brief On deallocating a chunk, decrease internal counter
     *
     */
    inline void on_deallocation(uint8_t*, std::size_t, const char*, int)
    {
        --num_allocs_;
    }

    /**
     * @brief Get the value of the counter
     *
     * @return int32_t
     */
    inline int32_t get_allocation_count() const
    {
        return num_allocs_;
    }

    /**
     * @brief Print a tracking report on the logger
     *
     */
    void report() const;

private:
    int32_t num_allocs_ = 0;
    std::string debug_name_;
    const kb::log::Channel* log_channel_ = nullptr;
};

} // namespace kb::memory::policy