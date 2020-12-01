#pragma once
#include "assert/assert.h"
#include "atomic_queue/atomic_queue.h"
#include "logger/logger.h"
#include "memory/heap_area.h"
#include "memory/memory.h"
#include "memory/pool_allocator.h"
#include "util/sparse_set.h"
#include <functional>

namespace kb
{
namespace th
{

// Brief idle/active timing stats per worker thread
#define PROFILING 1
// Attempt at reducing false sharing in shared state by page-aligning its members (TODO: measure this)
#define ENABLE_SHARED_STATE_PAGE_ALIGN 0

#if defined(__has_feature) && __has_feature(thread_sanitizer)
#define NO_SANITIZE __attribute__((no_sanitize("thread")))
#define ANNOTATE_HAPPENS_BEFORE(addr) AnnotateHappensBefore(__FILE__, __LINE__, static_cast<void*>(addr))
#define ANNOTATE_HAPPENS_AFTER(addr) AnnotateHappensAfter(__FILE__, __LINE__, static_cast<void*>(addr))
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
#else
#define NO_SANITIZE
#define ANNOTATE_HAPPENS_BEFORE(addr)
#define ANNOTATE_HAPPENS_AFTER(addr)
#endif

#if ENABLE_SHARED_STATE_PAGE_ALIGN
#define PAGE_ALIGN alignas(k_cache_line_size)
#else
#define PAGE_ALIGN
#endif

using JobHandle = std::size_t;
using JobKernel = std::function<void(void)>;
using tid_t = uint32_t;

// Maximum allowable number of worker threads
[[maybe_unused]] static constexpr size_t k_max_threads = 8;
// Maximum number of jobs per worker thread queue
[[maybe_unused]] static constexpr size_t k_max_jobs = 1024;
// Number of guard bits in a JobHandle
[[maybe_unused]] static constexpr size_t k_hnd_guard_bits = 48;
// Maximum number of stats packets in the monitor queue
[[maybe_unused]] static constexpr size_t k_stats_queue_capacity = 128;
// Maximum consecutive job resubmits due to pending parent, before thread is allowed to be rescheduled
[[maybe_unused]] static constexpr size_t k_max_push_pop_loop = 16;

using HandlePool = SecureSparsePool<JobHandle, k_max_jobs, k_hnd_guard_bits>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;
template <typename T> using JobQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, true, true, false, false>;
template <typename T>
using DeadJobQueue = atomic_queue::AtomicQueue<T, k_max_jobs * k_max_threads, T{}, true, true, false, false>;
template <typename T>
using ActivityQueue = atomic_queue::AtomicQueue2<T, k_stats_queue_capacity, true, true, false, false>;

struct WorkerActivity
{
    int64_t active_time_us = 0;
    int64_t idle_time_us = 0;
    size_t executed = 0;
    size_t stolen = 0;
    size_t resubmit = 0;
    tid_t tid = 0;

    inline void reset()
    {
        active_time_us = 0;
        idle_time_us = 0;
        executed = 0;
        stolen = 0;
        resubmit = 0;
    }
};

} // namespace th
} // namespace kb