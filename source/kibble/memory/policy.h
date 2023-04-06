#pragma once

#include "../logger2/logger.h"
#include <algorithm>
#include <cstdint>

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
    inline void put_sentinel_front(uint8_t *) const
    {
    }

    inline void put_sentinel_back(uint8_t *) const
    {
    }

    inline void check_sentinel_front(uint8_t *) const
    {
    }

    inline void check_sentinel_back(uint8_t *) const
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
    inline void put_sentinel_front(uint8_t *ptr) const
    {
        std::fill(ptr, ptr + SIZE_FRONT, 0xf0);
    }

    /**
     * @brief Write a 4-bytes back sentinel starting at ptr
     *
     * @param ptr start address to write to
     */
    inline void put_sentinel_back(uint8_t *ptr) const
    {
        std::fill(ptr, ptr + SIZE_BACK, 0x0f);
    }

    /**
     * @brief Assert that the front sentinel is intact
     *
     * @param ptr sentinel location
     */
    inline void check_sentinel_front([[maybe_unused]] uint8_t *ptr) const
    {
        K_ASSERT(*reinterpret_cast<uint32_t *>(ptr) == 0xf0f0f0f0, "Memory overwrite detected (front)", nullptr)
            .watch(static_cast<void *>(ptr))
            .watch(*reinterpret_cast<uint32_t *>(ptr));
    }

    /**
     * @brief Assert that the back sentinel is intact
     *
     * @param ptr sentinel location
     */
    inline void check_sentinel_back([[maybe_unused]] uint8_t *ptr) const
    {
        K_ASSERT(*reinterpret_cast<uint32_t *>(ptr) == 0x0f0f0f0f, "Memory overwrite detected (back)", nullptr)
            .watch(static_cast<void *>(ptr))
            .watch(*reinterpret_cast<uint32_t *>(ptr));
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
    inline void tag_allocation(uint8_t *, std::size_t) const
    {
    }
    inline void tag_deallocation(uint8_t *, std::size_t) const
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
    inline void on_allocation(uint8_t *, std::size_t, std::size_t) const
    {
    }
    inline void on_deallocation(uint8_t *) const
    {
    }
    inline int32_t get_allocation_count() const
    {
        return 0;
    }
    inline void report(const kb::log::Channel *) const
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
    /**
     * @brief On allocating a chunk, increase internal counter
     *
     */
    inline void on_allocation(uint8_t *, std::size_t, std::size_t)
    {
        ++num_allocs_;
    }

    /**
     * @brief On deallocating a chunk, decrease internal counter
     *
     */
    inline void on_deallocation(uint8_t *)
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
    inline void report(const kb::log::Channel *log_channel) const
    {
        if (num_allocs_)
        {
            klog(log_channel).uid("MemoryTracker").error("Alloc-dealloc mismatch: {}", num_allocs_);
        }
    }

private:
    int32_t num_allocs_ = 0;
};

} // namespace kb::memory::policy