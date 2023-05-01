#pragma once

#include "../logger2/logger.h"
#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace kb::memory::policy
{

/**
 * @brief Null MemoryArena thread-guard policy prescribed for single-threaded use cases.
 *
 */
class SingleThread
{
public:
    inline void enter() const
    {
    }

    inline void leave() const
    {
    }
};

/**
 * @brief MemoryArena thread-guard policy that can hold mutex-like primitives.
 * It will work as-is with std::mutex, but may need to be specialized for other synchronization primitives that do not
 * share the same API.
 *
 * @tparam SynchronizationPrimitiveT type of the synchronization primitive
 */
template <typename SynchronizationPrimitiveT>
class MultiThread
{
public:
    /**
     * @brief Lock the primitive on entering the thread guard
     *
     */
    inline void enter(void)
    {
        primitive_.lock();
    }

    /**
     * @brief Unlock the primitive on leaving the thread guard
     *
     */
    inline void leave(void)
    {
        primitive_.unlock();
    }

private:
    SynchronizationPrimitiveT primitive_;
};

/**
 * @brief Null bounds checking sanitization policy used by MemoryArena.
 *
 */
class NoBoundsChecking
{
public:
    inline void put_sentinel_front(uint8_t*) const
    {
    }

    inline void put_sentinel_back(uint8_t*) const
    {
    }

    inline void check_sentinel_front(uint8_t*) const
    {
    }

    inline void check_sentinel_back(uint8_t*) const
    {
    }

    static constexpr std::size_t SIZE_FRONT = 0;
    static constexpr std::size_t SIZE_BACK = 0;
};

/**
 * @brief Bounds checking sanitization policy used by MemoryArena.
 * This implementation will write 4-bytes sentinels at the supplied locations, and will be able to check if the
 * sentinels are still intact after some time, which allows to track memory overwrite bugs.
 *
 */
class SimpleBoundsChecking
{
public:
    /**
     * @brief Write a 4-bytes front sentinel starting at ptr
     *
     * @param ptr start address to write to
     */
    inline void put_sentinel_front(uint8_t* ptr) const
    {
        std::fill(ptr, ptr + SIZE_FRONT, 0xf0);
    }

    /**
     * @brief Write a 4-bytes back sentinel starting at ptr
     *
     * @param ptr start address to write to
     */
    inline void put_sentinel_back(uint8_t* ptr) const
    {
        std::fill(ptr, ptr + SIZE_BACK, 0x0f);
    }

    /**
     * @brief Assert that the front sentinel is intact
     *
     * @param ptr sentinel location
     */
    inline void check_sentinel_front([[maybe_unused]] uint8_t* ptr) const
    {
        K_ASSERT(*reinterpret_cast<uint32_t*>(ptr) == 0xf0f0f0f0, "Memory overwrite detected (front)", nullptr)
            .watch(static_cast<void*>(ptr))
            .watch(*reinterpret_cast<uint32_t*>(ptr));
    }

    /**
     * @brief Assert that the back sentinel is intact
     *
     * @param ptr sentinel location
     */
    inline void check_sentinel_back([[maybe_unused]] uint8_t* ptr) const
    {
        K_ASSERT(*reinterpret_cast<uint32_t*>(ptr) == 0x0f0f0f0f, "Memory overwrite detected (back)", nullptr)
            .watch(static_cast<void*>(ptr))
            .watch(*reinterpret_cast<uint32_t*>(ptr));
    }

    static constexpr std::size_t SIZE_FRONT = 4;
    static constexpr std::size_t SIZE_BACK = 4;
};

/**
 * @brief Null memory tagging policy.
 *
 */
class NoMemoryTagging
{
public:
    inline void tag_allocation(uint8_t*, std::size_t) const
    {
    }
    inline void tag_deallocation(uint8_t*, std::size_t) const
    {
    }
};

/**
 * @brief Null memory tracking policy
 *
 */
class NoMemoryTracking
{
public:
    NoMemoryTracking(const char*, const kb::log::Channel*)
    {
    }

    inline void on_allocation(uint8_t*, std::size_t, std::size_t, const char*, int) const
    {
    }
    inline void on_deallocation(uint8_t*, std::size_t, const char*, int) const
    {
    }
    inline int32_t get_allocation_count() const
    {
        return 0;
    }
    inline void report() const
    {
    }
};

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
    SimpleMemoryTracking(const char* debug_name, const kb::log::Channel* log_channel)
        : debug_name_(debug_name), log_channel_(log_channel)
    {
    }

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
    inline void report() const
    {
        if (num_allocs_)
        {
            klog(log_channel_)
                .uid("MemoryTracker")
                .error("Arena: {}, Alloc-dealloc mismatch: {}", debug_name_, num_allocs_);
        }
    }

private:
    int32_t num_allocs_ = 0;
    std::string debug_name_;
    const kb::log::Channel* log_channel_ = nullptr;
};

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

    VerboseMemoryTracking(const char* debug_name, const kb::log::Channel* log_channel)
        : debug_name_(debug_name), log_channel_(log_channel)
    {
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
                         debug_name_, utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin), alignment,
                         file, line);
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
                         debug_name_, utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin), file, line);
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

} // namespace kb::memory::policy