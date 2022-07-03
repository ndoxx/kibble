#include "thread/job/impl/worker.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/job_system.h"
#include "thread/sanitizer.h"
#include "time/clock.h"

namespace kb
{
namespace th
{

WorkerThread::WorkerThread(const WorkerProperties &props, JobSystem &jobsys)
    : props_(props), js_(jobsys), ss_(js_.get_shared_state())
{
#if K_PROFILE_JOB_SYSTEM
    activity_.tid = props_.tid;
#endif
}

void WorkerThread::spawn()
{
    // Generate list of stealable workers
    // Make sure that this worker cannot steal from itself
    for (tid_t tid = 0; tid < js_.get_threads_count(); ++tid)
        if (props_.tid != tid)
            stealable_workers_.push_back(&js_.get_worker(tid));

    // Spawn thread if it is not the main thread
    if (is_background())
        thread_ = std::thread(&WorkerThread::run, this);
}

void WorkerThread::join()
{
    if (is_background())
        thread_.join();
}

void WorkerThread::submit_public(Job *job)
{
    ANNOTATE_HAPPENS_BEFORE(&public_queue_); // Avoid false positives with TSan
    public_queue_.push(job);
}

void WorkerThread::submit_private(Job *job)
{
    ANNOTATE_HAPPENS_BEFORE(&private_queue_); // Avoid false positives with TSan
    private_queue_.push(job);
}

void WorkerThread::run()
{
    K_ASSERT(is_background(), "run() should not be called in the main thread.");

    while (ss_.running.load(std::memory_order_acquire))
    {
        state_.store(State::Running, std::memory_order_release);

        Job *job = nullptr;
        if (get_job(job))
        {
            process(job);
            continue;
        }

        state_.store(State::Idle, std::memory_order_release);
#if K_PROFILE_JOB_SYSTEM
        microClock clk;
#endif
        std::unique_lock<std::mutex> lock(ss_.wake_mutex);
        // The first condition in passed lambda avoids a possible deadlock, where a worker can
        // go to sleep with a non-empty queue and never wakes up, while the main thread waits for
        // the pending jobs it holds.
        // The second condition forces workers to wake up when the job system shuts down.
        // This avoids another deadlock on exit.
        ss_.cv_wake.wait(
            lock, [this]() { return !public_queue_.was_empty() || !ss_.running.load(std::memory_order_acquire); });
#if K_PROFILE_JOB_SYSTEM
        activity_.idle_time_us += clk.get_elapsed_time().count();
        js_.get_monitor().report_thread_activity(activity_);
        activity_.reset();
#endif
    }

    state_.store(State::Stopping, std::memory_order_release);
}

bool WorkerThread::get_job(Job *&job)
{
    // First, try to pop a job from the private queue, then the public queue, only then try to steal
    // Logical or is short-circuiting, only one job will be popped
    ANNOTATE_HAPPENS_AFTER(&private_queue_); // Avoid false positives with TSan
    ANNOTATE_HAPPENS_AFTER(&public_queue_);  // Avoid false positives with TSan
#ifdef K_ENABLE_WORK_STEALING
    return (private_queue_.try_pop(job) || public_queue_.try_pop(job) || steal_job(job));
#else
    return (private_queue_.try_pop(job) || public_queue_.try_pop(job));
#endif
}

#ifdef K_ENABLE_WORK_STEALING
bool WorkerThread::steal_job(Job *&job)
{
    for (size_t jj = 0; jj < props_.max_stealing_attempts; ++jj)
    {
        auto *p_worker = stealable_workers_[rr_next()];
        ANNOTATE_HAPPENS_AFTER(&p_worker->public_queue_); // Avoid false positives with TSan
        if (p_worker->public_queue_.try_pop(job))
        {
#if K_PROFILE_JOB_SYSTEM
            ++activity_.stolen;
#endif
            return true;
        }
    }
    return false;
}
#endif

void WorkerThread::process(Job *job)
{
    auto start = std::chrono::high_resolution_clock::now();

    try
    {
        job->kernel();
    }
    catch (...)
    {
        /*
            Store any exception thrown by the kernel function, it will be rethrown
            when the job is released.
            We should never get here when a non-void task is executed, as all
            exceptions thrown by the kernel are captured by the wrapper.
        */
        job->p_except = std::current_exception();
    }

    auto stop = std::chrono::high_resolution_clock::now();
    auto start_us = std::chrono::time_point_cast<std::chrono::microseconds>(start).time_since_epoch().count();
    auto stop_us = std::chrono::time_point_cast<std::chrono::microseconds>(stop).time_since_epoch().count();
    job->meta.execution_time_us = stop_us - start_us;

#if K_PROFILE_JOB_SYSTEM
    job->meta.start_timestamp_us = start_us;
    job->meta.thread_id = std::this_thread::get_id();
    activity_.active_time_us += job->meta.execution_time_us;
    ++activity_.executed;
#endif

    job->mark_processed();
    schedule_children(job);
    js_.release_job(job);
    ss_.pending.fetch_sub(1);
}

void WorkerThread::schedule_children(Job *job)
{
    for (Job *child : job->node)
    {
        /*
            If two parents finish at the same time, they could potentially schedule their
            children at the same time. The mark_scheduled() call makes sure that only
            one parent will get to schedule the children jobs.
        */
        if (child->is_ready() && child->mark_scheduled())
        {
            // Thread-safe call as long as the scheduler implementation is thread-safe
            js_.schedule(child);
#if K_PROFILE_JOB_SYSTEM
            ++activity_.scheduled;
#endif
        }
    }
}

bool WorkerThread::foreground_work()
{
    K_ASSERT(!is_background(), "foreground_work() should not be called in a background thread.");
    Job *job = nullptr;
    if (get_job(job))
    {
        process(job);
        return true;
    }

    return false;
}

} // namespace th
} // namespace kb