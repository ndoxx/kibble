#pragma once
#include "assert/assert.h"
#include "atomic_queue/atomic_queue.h"
#include "logger/logger.h"
#include "memory/atomic_pool_allocator.h"
#include "memory/heap_area.h"
#include "memory/memory.h"
#include "thread/job/config.h"

namespace kb
{
namespace th
{

using tid_t = uint32_t;
using worker_affinity_t = uint32_t;

template <typename T>
using JobQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, true, true, false, false>;

using JobPoolArena = memory::MemoryArena<memory::AtomicPoolAllocator<k_max_jobs * k_max_threads>,
                                         memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                                         memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;
template <typename T>
using ActivityQueue = atomic_queue::AtomicQueue2<T, k_stats_queue_capacity, true, true, false, false>;

/**
 * @internal
 * @brief Worker activity report struct.
 * Holds various statistics relative to a worker activity during last dispatch.
 * Used by the Monitor.
 *
 */
struct WorkerActivity
{
    /// Time in µs the worker was actively doing things
    int64_t active_time_us = 0;
    /// Time in µs the worker was doing nothing
    int64_t idle_time_us = 0;
    /// Number of tasks executed by the worker
    size_t executed = 0;
    /// Number of tasks stolen by the worker
    size_t stolen = 0;
    /// Number of children tasks scheduled by the worker
    size_t scheduled = 0;
    /// Worker id
    tid_t tid = 0;

    /**
     * @brief Reset all statistics
     *
     */
    inline void reset()
    {
        active_time_us = 0;
        idle_time_us = 0;
        executed = 0;
        stolen = 0;
        scheduled = 0;
    }
};

} // namespace th
} // namespace kb