#include "thread/job/job_system.h"
#include "logger/logger.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/worker.h"

#include <thread>

namespace kb
{
namespace th
{

// Maximal padding of a Job structure within the job pool
static constexpr size_t k_job_max_align = k_cache_line_size - 1;
// Total size of a Job node inside the pool
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;

JobSystem::JobSystem(memory::HeapArea &area, const JobSystemScheme &scheme)
    : scheme_(scheme), ss_(std::make_shared<SharedState>())
{
    KLOGN("thread") << "[JobSystem] Initializing." << std::endl;

    // Log scheme
    KLOG("thread", 0) << "Detail:" << std::endl;
    KLOGI << "Work stealing: " << (scheme_.enable_work_stealing ? "enabled" : "disabled") << std::endl;

    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    size_t max_threads = (scheme_.max_workers != 0) ? std::min(k_max_threads, scheme_.max_workers + 1) : k_max_threads;
    threads_count_ = std::min(max_threads, CPU_cores_count_);

    // Create monitor
    monitor_ = new Monitor(*this);

    // Create scheduler
    KLOGI << "Scheduler:     ";
    switch (scheme_.scheduling_algorithm)
    {
    case SchedulingAlgorithm::round_robin:
        KLOGI << "round-robin" << std::endl;
        scheduler_ = new RoundRobinScheduler(*this);
        break;
    case SchedulingAlgorithm::min_load:
        KLOGI << "minimum-load (dynamic)" << std::endl;
        scheduler_ = new MinimumLoadScheduler(*this);
        break;
    }

    // Init job pool
    KLOG("thread", 0) << "[JobSystem] Allocating job pool." << std::endl;
    ss_->job_pool.init(area, k_job_node_size + JobPoolArena::DECORATION_SIZE, "JobPool");

    // Spawn threads_count_-1 workers
    KLOGI << "Detected " << KS_VALU_ << CPU_cores_count_ << KC_ << " CPU cores." << std::endl;
    KLOGI << "Spawning " << KS_VALU_ << threads_count_ - 1 << KC_ << " worker threads." << std::endl;

    workers_.resize(threads_count_);
    for (uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        // TODO: Use K_NEW
        WorkerProperties props;
        props.is_background = (ii != 0);
        props.can_steal = scheme_.enable_work_stealing;
        props.max_stealing_attempts = scheme_.max_stealing_attempts;
        props.tid = ii;
        workers_[ii] = new WorkerThread(props, *this);
    }
    // Thread spawning is delayed to avoid a race condition of run() with other workers ctors
    for (auto *worker : workers_)
        worker->spawn();

    KLOGG("thread") << "[JobSystem] Ready." << std::endl;
}

JobSystem::~JobSystem()
{
    shutdown();
}

void JobSystem::shutdown()
{
    KLOGN("thread") << "[JobSystem] Shutting down." << std::endl;
    KLOGI << "Waiting for jobs to finish." << std::endl;
    wait();
    KLOGI << "All threads are joinable." << std::endl;

    // Notify all threads they are going to die
    ss_->running.store(false, std::memory_order_release);
    ss_->cv_wake.notify_all();
    for (auto *worker : workers_)
        worker->join();

#if K_PROFILE_JOB_SYSTEM
    // Log worker statistics
    monitor_->update_statistics();
    KLOGN("thread") << "[JobSystem] Thread statistics:" << std::endl;
    for (auto *thd : workers_)
        monitor_->log_statistics(thd->get_tid());
#endif

    // Save job execution profiles
    if (use_persistence_file_)
        monitor_->export_job_profiles(persistence_file_);

    // Cleanup
    for (auto *worker : workers_)
        delete worker;

    delete monitor_;
    delete scheduler_;

    KLOGG("thread") << "[JobSystem] Shutdown complete." << std::endl;
}

Job *JobSystem::create_job(JobKernel &&kernel, const JobMetadata &meta)
{
    Job *job = K_NEW_ALIGN(Job, ss_->job_pool, k_cache_line_size);
    job->kernel = std::move(kernel);
    job->meta = meta;
    return job;
}

void JobSystem::release_job(Job *job)
{
    // Make sure that the job was processed
    if (!job->finished.load(std::memory_order_acquire))
    {
        KLOGW("thread") << "Tried to release unprocessed job." << std::endl;
        return;
    }

    // Inform monitor about what happened with this job
    if (scheduler_->is_dynamic())
        monitor_->report_job_execution(job->meta);

    // Return job to the pool
    K_DELETE(job, ss_->job_pool);
}

void JobSystem::schedule(Job *job, tid_t caller_thread)
{
    // Sanity check
    if (!job->is_orphan.load())
    {
        KLOGW("thread") << "Tried to schedule child job #" << job->meta.label << std::endl;
        KLOGI << "Caller thread: " << caller_thread << std::endl;
        KLOGI << "Safely ignored." << std::endl;
        return;
    }

    // Increment job count, dispatch and wake up workers
    ss_->pending.fetch_add(1);
    scheduler_->dispatch(job, caller_thread);
    ss_->cv_wake.notify_all();
}

void JobSystem::use_persistence_file(const fs::path &filepath)
{
    persistence_file_ = filepath;
    use_persistence_file_ = true;
    monitor_->load_job_profiles(persistence_file_);
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

bool JobSystem::is_work_done(Job *job) const
{
    return job->finished.load(std::memory_order_acquire);
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
#if K_PROFILE_JOB_SYSTEM
    int64_t idle_time_us = 0;
#endif
    while (condition())
    {
        if (!workers_[0]->foreground_work())
        {
            // There's nothing we can do, just wait. Some work may come to us.
#if K_PROFILE_JOB_SYSTEM
            microClock clk;
#endif
            ss_->cv_wake.notify_all(); // wake worker threads
            std::this_thread::yield(); // allow this thread to be rescheduled
#if K_PROFILE_JOB_SYSTEM
            idle_time_us += clk.get_elapsed_time().count();
#endif
        }
    }

#if K_PROFILE_JOB_SYSTEM
    auto &activity = workers_[0]->get_activity();
    activity.idle_time_us += idle_time_us;
    monitor_->report_thread_activity(activity);
    activity.reset();
    monitor_->update_statistics();
#endif
}

void JobSystem::wait(std::function<bool()> condition)
{
    wait_until([this, &condition]() { return is_busy() && condition(); });
    if (!condition())
    {
        KLOG("thread", 0) << "[JobSystem] wait() exited early." << std::endl;
    }
    if (!is_busy())
        monitor_->wrap();
}

void JobSystem::wait_for(Job *job, std::function<bool()> condition)
{
    wait_until([this, &condition, job]() { return !is_work_done(job) && condition(); });
    if (!condition())
    {
        KLOG("thread", 0) << "[JobSystem] wait_for() exited early." << std::endl;
    }
}

std::vector<WorkerThread *> JobSystem::get_compatible_workers(worker_affinity_t affinity)
{
    std::vector<WorkerThread *> ret;
    for (uint32_t ii = 0; ii < workers_.size(); ++ii)
        if (affinity & (1 << ii))
            ret.push_back(workers_[ii]);

    return ret;
}

std::vector<tid_t> JobSystem::get_compatible_worker_ids(worker_affinity_t affinity)
{
    std::vector<tid_t> ret;
    for (tid_t ii = 0; ii < workers_.size(); ++ii)
        if (affinity & (1 << ii))
            ret.push_back(ii);

    return ret;
}

void Task<void>::release()
{
    if (js_ != nullptr && job_ != nullptr)
    {
        auto p_except = job_->p_except;
        js_->release_job(job_);
        if (p_except != nullptr)
            std::rethrow_exception(p_except);
    }
}

Task<void>::Task(JobSystem *js, JobKernel &&kernel, const JobMetadata &meta) : js_(js)
{
    job_ = js->create_job(std::forward<JobKernel>(kernel), meta);
}

} // namespace th
} // namespace kb