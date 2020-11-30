#include "thread/job.h"
#include "thread/impl/monitor.h"
#include "thread/impl/scheduler.h"
#include "thread/impl/worker.h"

#include <algorithm>

namespace kb
{

// Size of a cache line -> controlling alignment prevents false sharing
static constexpr size_t k_cache_line_size = 64;
// Maximal padding of a Job structure within the job pool
static constexpr size_t k_job_max_align = k_cache_line_size - 1;
// Total size of a Job node inside the pool
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;

// Helper struct to show a handle composition
struct DisplayHandle
{
    DisplayHandle(JobHandle handle)
    {
        guard = HandlePool::guard_value(handle);
        naked = HandlePool::unguard(handle);
    }

    friend std::ostream& operator<<(std::ostream& stream, const DisplayHandle& dh);

    size_t guard;
    size_t naked;
};

std::ostream& operator<<(std::ostream& stream, const DisplayHandle& dh)
{
    stream << dh.naked << '/' << dh.guard;
    return stream;
}

JobSystem::JobSystem(memory::HeapArea& area, const JobSystemScheme& scheme)
    : scheme_(scheme), ss_(std::make_shared<SharedState>())
{
    KLOGN("thread") << "[JobSystem] Initializing." << std::endl;

    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    if(scheme_.max_threads == 0)
        threads_count_ =
            std::min(k_max_threads, std::max(1ul, CPU_cores_count_ - size_t(!scheme_.enable_foreground_work)));
    else
        threads_count_ = std::min(k_max_threads, scheme_.max_threads + size_t(scheme_.enable_foreground_work));

    // Create monitor
    monitor_ = new Monitor(*this);

    // Create scheduler
    switch(scheme_.scheduling_algorithm)
    {
    case SchedulingAlgorithm::round_robin:
        KLOG("thread", 1) << "[JobSystem] Using round-robin scheduler." << std::endl;
        scheduler_ = new RoundRobinScheduler(*this);
        break;
    case SchedulingAlgorithm::min_load:
        KLOG("thread", 1) << "[JobSystem] Using minimum-load dynamic scheduler." << std::endl;
        scheduler_ = new MininmumLoadScheduler(*this);
        break;
    default:
        KLOGW("thread") << "Unknown scheduling algorithm, fallback: round-robin" << std::endl;
        scheduler_ = new RoundRobinScheduler(*this);
    }

    // Spawn workers
    KLOG("thread", 1) << "[JobSystem] Allocating job pool." << std::endl;
    ss_->job_pool.init(area, k_job_node_size + PoolArena::DECORATION_SIZE, k_max_jobs * threads_count_, "JobPool");

    KLOG("thread", 1) << "[JobSystem] Spawning worker threads." << std::endl;
    KLOGI << "Detected " << WCC('v') << CPU_cores_count_ << WCC(0) << " CPU cores." << std::endl;
    KLOGI << "Spawning " << WCC('v') << threads_count_ - size_t(scheme_.enable_foreground_work) << WCC(0)
          << " worker threads." << std::endl;
    if(scheme_.enable_foreground_work)
    {
        KLOGI << "Worker " << WCC('v') << 0 << WCC(0) << " is foreground." << std::endl;
    }

    threads_.reserve(threads_count_);
    for(uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        auto* worker =
            new WorkerThread(ii, (ii != 0) || !scheme_.enable_foreground_work, scheme_.enable_work_stealing, *this);
        threads_.push_back(worker);
    }
    // Thread spawning is delayed to avoid a race condition of run() with tha atomic queue's ctor on memset
    for(auto* thd : threads_)
        thd->spawn();

    KLOGG("thread") << "JobSystem ready." << std::endl;
}

JobSystem::~JobSystem() { shutdown(); }

void JobSystem::shutdown()
{
    KLOGN("thread") << "[JobSystem] Shutting down." << std::endl;
    KLOGI << "Waiting for jobs to finish." << std::endl;
    wait();
    KLOGI << "All threads are joinable." << std::endl;

    // Notify all threads they are going to die
    ss_->running.store(false, std::memory_order_release);
    ss_->cv_wake.notify_all();
    for(auto* thd : threads_)
        thd->join();

    cleanup();

#if PROFILING
    for(auto* thd : threads_)
        monitor_->log_statistics(thd->get_tid());
#endif

    for(auto* thd : threads_)
        delete thd;

    delete monitor_;
    delete scheduler_;

    KLOGG("thread") << "JobSystem shutdown complete." << std::endl;
}

void JobSystem::cleanup()
{
    Job* job = nullptr;
    bool dynamic_scheduling = scheduler_->is_dynamic();
    while(ss_->dead_jobs.try_pop(job))
    {
        // Inform monitor about what happened with this job
        if(dynamic_scheduling)
            monitor_->report_job_execution(job->metadata);
        // Return job to the pool
        K_DELETE(job, ss_->job_pool);
    }
}

JobHandle JobSystem::dispatch(JobFunction&& function, uint64_t label, ExecutionPolicy policy)
{
    K_ASSERT(!(!scheme_.enable_foreground_work && (policy == ExecutionPolicy::deferred)),
             "Cannot execute job synchronously: foreground work is disabled.");

    JobHandle handle;
    {
        const std::scoped_lock<SpinLock> lock(ss_->handle_lock);
        handle = ss_->handle_pool.acquire();
    }

    Job* job = K_NEW_ALIGN(Job, ss_->job_pool, k_cache_line_size);
    job->function = std::move(function);
    job->handle = handle;
    job->metadata.label = label;
    job->metadata.execution_policy = static_cast<SchedulerExecutionPolicy>(policy);
    ss_->pending.fetch_add(1);

    scheduler_->schedule(job);

    return handle;
}

JobHandle JobSystem::async(JobFunction&& function, uint64_t label)
{
    cleanup();
    auto handle = dispatch(std::move(function), label, ExecutionPolicy::async);
    ss_->cv_wake.notify_all();
    return handle;
}

void JobSystem::update()
{
    // Empty the dead job queue
    cleanup();
    // Submit all scheduled jobs to workers
    scheduler_->submit();
    // Wake all worker threads
    ss_->cv_wake.notify_all();
}

// Main thread atomically increments pending each time a job is pushed to the queue.
// Worker threads atomically decrement pending each time they finished a job.
// Then we just need to wait for pending to return to zero in order
// to be sure all worker threads have finished.
bool JobSystem::is_busy() const { return ss_->pending.load(std::memory_order_relaxed) > 0; }

bool JobSystem::is_work_done(JobHandle handle) const { return !ss_->handle_pool.is_valid(handle); }

// NOTE(ndx): Instead of busy-waiting I tried
//      std::unique_lock<std::mutex> lock(ss_->wait_mutex);
//      ss_->cv_wait.wait(lock, [this]() { return !is_busy(); });
// and in WorkerThread::execute() I do this after the fetch_sub:
//      ss_.cv_wait.notify_one();
// But it deadlocks (lost wakeups?)
void JobSystem::wait_untill(std::function<bool()> condition)
{
    // Do some work
    if(scheme_.enable_foreground_work)
        while(condition())
            if(!threads_[0]->foreground_work())
                break;

#if PROFILING
    microClock clk;
#endif
    // Poll
    while(condition())
    {
        ss_->cv_wake.notify_all(); // wake worker threads
        std::this_thread::yield(); // allow this thread to be rescheduled
    }
#if PROFILING
    if(scheme_.enable_foreground_work)
    {
        auto& activity = threads_[0]->get_activity();
        activity.idle_time_us += clk.get_elapsed_time().count();
        monitor_->report_thread_activity(activity);
        activity.reset();
    }
    monitor_->update_statistics();
#endif
}

void JobSystem::wait(std::function<bool()> condition)
{
    wait_untill([this, &condition]() { return is_busy() && condition(); });
    if(!condition())
    {
        KLOG("thread", 0) << "[JobSystem] wait() exited early." << std::endl;
    }
    if(!is_busy())
        monitor_->wrap();
}

void JobSystem::wait_for(JobHandle handle, std::function<bool()> condition)
{
    wait_untill([this, &condition, handle]() { return !is_work_done(handle) && condition(); });
    if(!condition())
    {
        KLOG("thread", 0) << "[JobSystem] wait_for() exited early." << std::endl;
    }
}

} // namespace kb