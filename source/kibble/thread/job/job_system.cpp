#include "thread/job/job_system.h"
#include "assert/assert.h"
#include "logger2/logger.h"
#include "math/constexpr_math.h"
#include "memory/arena.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/worker.h"
#include "time/instrumentation.h"

#include "fmt/std.h"
#include <thread>

namespace kb
{
namespace th
{

inline size_t worker_count(const JobSystemScheme& scheme, size_t CPU_cores)
{
    size_t max_threads = (scheme.max_workers != 0) ? std::min(k_max_threads, scheme.max_workers + 1) : k_max_threads;
    return std::min(max_threads, CPU_cores);
}

inline size_t local_arena_requirements(const JobSystemScheme& scheme)
{
    // Allocation size for barriers
    size_t barrier_alloc_size = sizeof(Barrier) * scheme.max_barriers + kb::memory::k_cache_line_size;

    // Allocation size for workers
    size_t workers = worker_count(scheme, std::thread::hardware_concurrency());
    size_t worker_alloc_size = sizeof(WorkerThread) * workers + kb::memory::k_cache_line_size;

    return barrier_alloc_size + worker_alloc_size;
}

JobMetadata::JobMetadata(worker_affinity_t affinity, const std::string& profile_name) : worker_affinity(affinity)
{
    name = profile_name;
}

size_t JobSystem::get_memory_requirements(const JobSystemScheme& scheme)
{
    // Total size of a Job node inside the pool
    // Jobs are always aligned to the cache line, so we want the nearest multiple of alignment
    // and we also have to take into account additional data written by the allocator
    constexpr size_t k_job_node_size =
        math::round_up_pow2(sizeof(Job) + JobPoolArena::k_allocation_overhead, kb::memory::k_cache_line_size);

    // Area will need enough space for each job
    // Also add a cache line, to account for heap area block alignment
    size_t job_alloc_size = k_max_jobs * k_max_threads * k_job_node_size + kb::memory::k_cache_line_size;

    return local_arena_requirements(scheme) + job_alloc_size;
}

JobSystem::JobSystem(memory::HeapArea& area, const JobSystemScheme& scheme, const kb::log::Channel* log_channel)
    : scheme_(scheme), arena_("JobSystemLocalArena", area, local_arena_requirements(scheme)),
      ss_(std::make_unique<SharedState>(area)), scheduler_(new Scheduler(*this)), monitor_(new Monitor(*this)),
      log_channel_(log_channel)
{
    klog(log_channel_).uid("JobSystem").info("Initializing.");

    // * Allocate barriers
    barriers_ = K_NEW_ARRAY_DYNAMIC(Barrier, scheme.max_barriers, arena_);

    // * Allocate workers
    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    threads_count_ = worker_count(scheme, CPU_cores_count_);

    klog(log_channel_).uid("JobSystem").debug("Detected {} CPU cores.", CPU_cores_count_);
    klog(log_channel_).uid("JobSystem").debug("Spawning {} (async) worker threads.", threads_count_ - 1);

    if (threads_count_ == 1)
    {
        klog(log_channel_)
            .uid("JobSystem")
            .warn("Tasks marked with WORKER_AFFINITY_ASYNC will be scheduled to the main thread.");
    }

    workers_ = K_NEW_ARRAY_DYNAMIC(WorkerThread, threads_count_, arena_);

    // * Spawn workers
    for (uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        WorkerProperties props;
        props.max_stealing_attempts = scheme_.max_stealing_attempts;
        props.tid = ii;

        auto& worker = workers_[ii];
        worker.spawn(this, &(*ss_), props);
        auto native_id = worker.is_background() ? worker.get_native_thread_id() : std::this_thread::get_id();
        thread_ids_.insert({native_id, worker.get_tid()});
        klog(log_channel_)
            .uid("JobSystem")
            .verbose("Spawned worker #{}, native thread id: {}", worker.get_tid(), native_id);
    }

    klog(log_channel_).uid("JobSystem").debug("Ready.");
}

JobSystem::~JobSystem()
{
    // * Shutdown thread pool
    shutdown();

    // * Destroy workers
    K_DELETE_ARRAY(workers_, arena_);

    // * Destroy barriers
    for (barrier_t id = 0; id < scheme_.max_barriers; ++id)
    {
        K_ASSERT(!barriers_[id].is_used(), "Barrier still in use.", log_channel_).watch_var__(id, "Barrier index");
    }

    K_DELETE_ARRAY(barriers_, arena_);
}

void JobSystem::shutdown()
{
    klog(log_channel_).uid("JobSystem").info("Shutting down.");
    klog(log_channel_).uid("JobSystem").debug("Waiting for jobs to finish.");
    wait();

    // Notify all threads they are going to die
    ss_->running.store(false, std::memory_order_release);
    ss_->cv_wake.notify_all();
    for (size_t idx = 0; idx < threads_count_; ++idx)
    {
        workers_[idx].join();
    }

    // We just killed all threads, including the logger thread
    // So we must go back to sync mode
    kb::log::Channel::set_async(nullptr);
    klog(log_channel_).uid("JobSystem").debug("All threads have joined.");

#ifdef K_USE_JOB_SYSTEM_PROFILING
    // Log worker statistics
    monitor_->update_statistics();
    klog(log_channel_).uid("JobSystem").verbose("Thread statistics:");
    for (size_t idx = 0; idx < threads_count_; ++idx)
    {
        monitor_->log_statistics(workers_[idx].get_tid(), log_channel_);
    }
#endif

    klog(log_channel_).uid("JobSystem").info("Shutdown complete.");
}

barrier_t JobSystem::create_barrier()
{
    // Find first unused barrier
    for (barrier_t id = 0; id < scheme_.max_barriers; ++id)
    {
        bool expected{false};
        if (barriers_[id].mark_used(expected, true))
        {
            return id;
        }
    }

    // No unused barrier found
    return k_no_barrier;
}

void JobSystem::destroy_barrier(barrier_t id)
{
    K_ASSERT(id < scheme_.max_barriers, "Barrier index out of bounds.", log_channel_);
    Barrier& barrier = barriers_[id];
    // Check that barrier has no pending jobs
    K_ASSERT(barrier.finished(), "Tried to destroy barrier with unfinished jobs.", log_channel_);
    // Mark barrier as unused
    bool expected{true};
    barrier.mark_used(expected, false);
    K_ASSERT(expected, "Tried to destroy unused barrier.", log_channel_);
}

Barrier& JobSystem::get_barrier(barrier_t id)
{
    K_ASSERT(id < scheme_.max_barriers, "Barrier index out of bounds.", log_channel_);
    return barriers_[id];
}

Job* JobSystem::create_job(JobKernel&& kernel, JobMetadata&& meta)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    Job* job = K_NEW(Job, ss_->job_pool);
    job->kernel = std::move(kernel);
    job->meta = std::move(meta);
    return job;
}

void JobSystem::release_job(Job* job)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    // Make sure that the job was processed
    K_ASSERT(job->is_processed(), "Tried to release unprocessed job.", log_channel_);

    // Return job to the pool
    K_DELETE(job, ss_->job_pool);
}

void JobSystem::schedule(Job* job)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    // Sanity check
    K_ASSERT(job->is_ready(), "Tried to schedule job with unfinished dependencies.", log_channel_);

    // Setup barrier if any
    if (job->barrier_id != k_no_barrier)
    {
        get_barrier(job->barrier_id).add_dependency();
    }

    // Increment job count, dispatch and wake up workers
    ss_->pending.fetch_add(1, std::memory_order_release);
    scheduler_->dispatch(job);
    ss_->cv_wake.notify_all();
}

// Main thread and workers (on rescheduling) atomically increment pending each
// time a job is pushed to the queue.
// Main thread and workers atomically decrement pending each time they finished a job.
// Then we just need to wait for pending to return to zero in order
// to be sure all worker threads have finished.
bool JobSystem::is_busy() const
{
    return ss_->pending.load(std::memory_order_acquire) > 0;
}

bool JobSystem::is_work_done(Job* job) const
{
    return job->is_processed();
}

// NOTE(ndx): Instead of busy-waiting I tried
//      std::unique_lock<std::mutex> lock(ss_->wait_mutex);
//      ss_->cv_wait.wait(lock, [this]() { return !is_busy(); });
// and in WorkerThread::execute() I do this after the fetch_sub:
//      ss_.cv_wait.notify_one();
// But it deadlocks (lost wakeups?)
void JobSystem::wait_until(std::function<bool()> condition)
{
    // Do some work to assist worker threads
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());
#ifdef K_USE_JOB_SYSTEM_PROFILING
    int64_t idle_time_us = 0;
#endif

    while (condition())
    {
        if (!workers_[0].foreground_work())
        {
            // There's nothing we can do, just wait. Some work may come to us.
#ifdef K_USE_JOB_SYSTEM_PROFILING
            microClock clk;
#endif
            ss_->cv_wake.notify_all(); // wake worker threads
            std::this_thread::yield(); // allow this thread to be rescheduled
#ifdef K_USE_JOB_SYSTEM_PROFILING
            idle_time_us += clk.get_elapsed_time().count();
#endif
        }
    }

#ifdef K_USE_JOB_SYSTEM_PROFILING
    auto& activity = workers_[0].get_activity();
    activity.idle_time_us += idle_time_us;
    monitor_->report_thread_activity(activity);
    activity.reset();
    monitor_->update_statistics();
#endif
}

WorkerThread& JobSystem::get_worker(size_t idx)
{
    return workers_[idx];
}

/// Get the worker at input index (const).
const WorkerThread& JobSystem::get_worker(size_t idx) const
{
    return workers_[idx];
}

void JobSystem::abort()
{
    // Join all workers as fast as possible
    try
    {
        ss_->running.store(false, std::memory_order_release);
        ss_->cv_wake.notify_all();
        for (size_t idx = 0; idx < threads_count_; ++idx)
        {
            workers_[idx].join();
        }
    }
    catch (const std::exception&)
    {
    }

    // We just killed all threads, including the logger thread
    // So we must go back to sync mode
    kb::log::Channel::set_async(nullptr);
    klog(log_channel_).uid("JobSystem").warn("PANIC: Essential work transfered to caller thread.");

    // Execute essential work on the caller thread
    for (size_t idx = 0; idx < threads_count_; ++idx)
    {
        workers_[idx].panic();
    }

    klog(log_channel_).uid("JobSystem").info("Shutting down.");

    exit(0);
}

} // namespace th
} // namespace kb