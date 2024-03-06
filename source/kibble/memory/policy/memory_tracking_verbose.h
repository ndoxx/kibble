#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "../../logger2/logger.h"
#include "../memory_utils.h"

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

    inline void init(const std::string& debug_name, const kb::log::Channel* log_channel)
    {
        debug_name_ = debug_name;
        log_channel_ = log_channel;
    }

    /**
     * @brief On allocating a chunk, save alloc info
     *
     */
    inline void on_allocation(uint8_t* begin, std::size_t decorated_size, std::size_t alignment, const char* file,
                              int line)
    {
        ++num_allocs_;
        allocations_.insert({reinterpret_cast<size_t>(begin), {begin, decorated_size, alignment, file, line}});

        if (log_channel_)
        {
            klog(log_channel_)
                .uid("Arena")
                .verbose(R"({} -- Allocation:
Decorated size: {}
Begin ptr:      {:#x}
Alignment:      {}B,
Location:       {}:{})",
                         debug_name_, kb::memory::utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin),
                         alignment, file, line);
        }
    }

    /**
     * @brief On deallocating a chunk, erase alloc info
     *
     */
    inline void on_deallocation(uint8_t* begin, std::size_t decorated_size, const char* file, int line)
    {
        --num_allocs_;
        if (auto it = allocations_.find(reinterpret_cast<size_t>(begin)); it != allocations_.end())
        {
            allocations_.erase(it);
        }

        if (log_channel_)
        {
            klog(log_channel_)
                .uid("Arena")
                .verbose(R"({} -- Deallocation:
Decorated size: {}
Begin ptr:      {:#x}
Location:       {}:{})",
                         debug_name_, kb::memory::utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin),
                         file, line);
        }
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
    inline void report() const
    {
        if (num_allocs_)
        {
            klog(log_channel_)
                .uid("MemoryTracker")
                .error("Arena: {}, Alloc-dealloc mismatch: {}", debug_name_, num_allocs_);

            for (auto&& [key, info] : allocations_)
            {
                klog(log_channel_)
                    .uid("MemoryTracker")
                    .info(R"(Unresolved:
begin: {}
decorated size: {}
alignment: {}
location: {}:{})",
                          reinterpret_cast<void*>(info.begin), info.decorated_size, info.alignment, info.file,
                          info.line);
            }
        }
    }

private:
    int32_t num_allocs_ = 0;
    std::unordered_map<size_t, AllocInfo> allocations_;
    std::string debug_name_;
    const kb::log::Channel* log_channel_ = nullptr;
};

} // namespace kb::policy