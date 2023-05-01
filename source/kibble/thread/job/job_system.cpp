#include "thread/job/job_system.h"
#include "logger2/logger.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/worker.h"
#include "time/instrumentation.h"

#include <fmt/std.h>
#include <thread>

namespace kb
{
namespace th
{

// Maximal padding of a Job structure within the job pool
static constexpr size_t k_job_max_align = k_cache_line_size - 1;
// Total size of a Job node inside the pool
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;

JobMetadata::JobMetadata(worker_affinity_t affinity, const std::string& profile_name) : worker_affinity(affinity)
{
    name = profile_name;
}

size_t JobSystem::get_memory_requirements()
{
    // Area will need to contain the memory arena, and enough space for each job
    return sizeof(JobPoolArena) + k_max_threads * k_max_jobs * (k_job_node_size + JobPoolArena::DECORATION_SIZE);
}

JobSystem::JobSystem(memory::HeapArea& area, const JobSystemScheme& scheme, const kb::log::Channel* log_channel)
    : scheme_(scheme), scheduler_(new Scheduler(*this)), monitor_(new Monitor(*this)),
      ss_(std::make_shared<SharedState>()), log_channel_(log_channel)
{
    klog(log_channel_).uid("JobSystem").info("Initializing.");

    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    size_t max_threads = (scheme_.max_workers != 0) ? std::min(k_max_threads, scheme_.max_workers + 1) : k_max_threads;
    threads_count_ = std::min(max_threads, CPU_cores_count_);

    // Init job pool
    klog(log_channel_).uid("JobSystem").debug("Allocating job pool.");
    ss_->job_pool =
        std::make_shared<JobPoolArena>("JobPool", nullptr, area, k_job_node_size + JobPoolArena::DECORATION_SIZE);

    // Spawn threads_count_-1 workers
    klog(log_channel_).uid("JobSystem").debug("Detected {} CPU cores.", CPU_cores_count_);
    klog(log_channel_).uid("JobSystem").debug("Spawning {} worker threads.", threads_count_ - 1);

    if (threads_count_ == 1)
    {
        klog(log_channel_)
            .uid("JobSystem")
            .warn("Tasks marked with WORKER_AFFINITY_ASYNC will be scheduled to the main thread.");
    }

    workers_.reserve(threads_count_);
    for (uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        // TODO: Use K_NEW
        WorkerProperties props;
        props.max_stealing_attempts = scheme_.max_stealing_attempts;
        props.tid = ii;
        workers_.push_back(std::make_unique<WorkerThread>(props, *this));
    }
    // Thread spawning is delayed to avoid a race condition of run() with other workers ctors
    for (auto& worker : workers_)
    {
        worker->spawn();
        auto native_id = worker->is_background() ? worker->get_native_thread_id() : std::this_thread::get_id();
        thread_ids_.insert({native_id, worker->get_tid()});
        klog(log_channel_)
            .uid("JobSystem")
            .verbose("Spawned worker #{}, native thread id: {}", worker->get_tid(), native_id);
    }

    klog(log_channel_).uid("JobSystem").debug("Ready.");
}

JobSystem::~JobSystem()
{
    shutdown();
}

void JobSystem::shutdown()
{
    klog(log_channel_).uid("JobSystem").info("Shutting down.");
    klog(log_channel_).uid("JobSystem").debug("Waiting for jobs to finish.");
    wait();

    // Notify all threads they are going to die
    ss_->running.store(false, std::memory_order_release);
    ss_->cv_wake.notify_all();
    for (auto& worker : workers_)
        worker->join();

    // We just killed all threads, including the logger thread
    // So we must go back to sync mode
    kb::log::Channel::set_async(nullptr);
    klog(log_channel_).uid("JobSystem").debug("All threads have joined.");

#ifdef K_PROFILE_JOB_SYSTEM
    // Log worker statistics
    monitor_->update_statistics();
    klog(log_channel_).uid("JobSystem").verbose("Thread statistics:");
    for (auto& worker : workers_)
        monitor_->log_statistics(worker->get_tid(), log_channel_);
#endif

    klog(log_channel_).uid("JobSystem").info("Shutdown complete.");
}

Job* JobSystem::create_job(JobKernel&& kernel, JobMetadata&& meta)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    auto& pool = *(ss_->job_pool);
    Job* job = K_NEW_ALIGN(Job, pool, k_cache_line_size);
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
    auto& pool = *(ss_->job_pool);
    K_DELETE(job, pool);
}

void JobSystem::schedule(Job* job)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    // Sanity check
    K_ASSERT(job->is_ready(), "Tried to schedule job with unfinished dependencies.", log_channel_);

    // Increment job count, dispatch and wake up workers
    ss_->pending.fetch_add(1);
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
#ifdef K_PROFILE_JOB_SYSTEM
    int64_t idle_time_us = 0;
#endif

    while (condition())
    {
        if (!workers_[0]->foreground_work())
        {
            // There's nothing we can do, just wait. Some work may come to us.
#ifdef K_PROFILE_JOB_SYSTEM
            microClock clk;
#endif
            ss_->cv_wake.notify_all(); // wake worker threads
            std::this_thread::yield(); // allow this thread to be rescheduled
#ifdef K_PROFILE_JOB_SYSTEM
            idle_time_us += clk.get_elapsed_time().count();
#endif
        }
    }

#ifdef K_PROFILE_JOB_SYSTEM
    auto& activity = workers_[0]->get_activity();
    activity.idle_time_us += idle_time_us;
    monitor_->report_thread_activity(activity);
    activity.reset();
    monitor_->update_statistics();
#endif
}

void JobSystem::abort()
{
    // Join all workers as fast as possible
    try
    {
        ss_->running.store(false, std::memory_order_release);
        ss_->cv_wake.notify_all();
        for (auto& worker : workers_)
            worker->join();
    }
    catch (const std::exception&)
    {
    }

    // We just killed all threads, including the logger thread
    // So we must go back to sync mode
    kb::log::Channel::set_async(nullptr);
    klog(log_channel_).uid("JobSystem").warn("PANIC: Essential work transfered to caller thread.");

    // Execute essential work on the caller thread
    for (auto& worker : workers_)
        worker->panic();

    klog(log_channel_).uid("JobSystem").info("Shutting down.");

    exit(0);
}

} // namespace th
} // namespace kb