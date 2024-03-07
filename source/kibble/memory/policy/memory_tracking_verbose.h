#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

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

class VerboseMemoryTracking
{
public:
    struct AllocInfo
    {
        uint8_t* begin;
        std::size_t decorated_size;
        std::size_t alignment;
        const char* file;
        int line;
    };

    void init(const std::string& debug_name, const HeapArea& area);

    /**
     * @brief On allocating a chunk, save alloc info
     *
     */
    void on_allocation(uint8_t* begin, std::size_t decorated_size, std::size_t alignment, const char* file, int line);

    /**
     * @brief On deallocating a chunk, erase alloc info
     *
     */
    void on_deallocation(uint8_t* begin, std::size_t decorated_size, const char* file, int line);

    /**
     * @brief Print a tracking report on the logger
     *
     */
    void report() const;

    /**
     * @brief Get the value of the counter
     *
     * @return int32_t
     */
    inline int32_t get_allocation_count() const
    {
        return num_allocs_;
    }

private:
    int32_t num_allocs_ = 0;
    std::unordered_map<size_t, AllocInfo> allocations_;
    std::string debug_name_;
    const kb::log::Channel* log_channel_ = nullptr;
};

} // namespace kb::memory::policy