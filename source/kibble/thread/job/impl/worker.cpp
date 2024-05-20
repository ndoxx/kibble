#include "thread/job/impl/worker.h"
#include "../../../util/sanitizer.h"
#include "assert/assert.h"
#include "thread/job/impl/barrier.h"
#include "thread/job/impl/job.h"
#include "thread/job/impl/job_graph.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/job_system.h"
#include "time/clock.h"
#include "time/instrumentation.h"

namespace kb::th
{

void WorkerThread::spawn(JobSystem* js, SharedState* ss, const WorkerProperties& props)
{
    js_ = js;
    ss_ = ss;
    props_ = props;
#ifdef K_USE_JOB_SYSTEM_PROFILING
    activity_.tid = props_.tid;
#endif

    // Generate list of stealable workers
    // Make sure that this worker cannot steal from itself
    for (tid_t tid = 0; tid < js_->get_threads_count(); ++tid)
        if (props_.tid != tid)
            stealable_workers_.push_back(tid);

    // Spawn thread if it is not the main thread
    if (is_background())
        thread_ = std::thread(&WorkerThread::run, this);
}

void WorkerThread::join()
{
    if (is_background())
        thread_.join();
}

void WorkerThread::submit(Job* job, bool stealable)
{
    size_t idx = size_t(!stealable);
    ANNOTATE_HAPPENS_BEFORE(&queues_[idx]); // Avoid false positives with TSan
    queues_[idx].push(job);
}

void WorkerThread::run()
{
    K_ASSERT(is_background(), "run() should not be called in the main thread.", nullptr);

    while (ss_->running.load(std::memory_order_acquire))
    {
        state_.store(State::Running, std::memory_order_release);

        Job* job = nullptr;
        if (get_job(job))
        {
            JobState expected = JobState::Pending;
            if (job->exchange_state(expected, JobState::Executing))
            {
                process(job);
            }
            continue;
        }

        state_.store(State::Idle, std::memory_order_release);
#ifdef K_USE_JOB_SYSTEM_PROFILING
        microClock clk;
#endif
        std::unique_lock<std::mutex> lock(ss_->wake_mutex);
        /*
            The first condition in passed lambda avoids a possible deadlock, where a worker can
            go to sleep with a non-empty queue and never wakes up, while the main thread waits for
            the pending jobs it holds.
            The second condition forces workers to wake up when the job system shuts down.
            This avoids another deadlock on exit.
        */
        ss_->cv_wake.wait(lock,
                          [this]() { return had_pending_jobs() || !ss_->running.load(std::memory_order_acquire); });

#ifdef K_USE_JOB_SYSTEM_PROFILING
        activity_.idle_time_us += clk.get_elapsed_time().count();
        js_->get_monitor().report_thread_activity(activity_);
        activity_.reset();
#endif
    }

    state_.store(State::Stopping, std::memory_order_release);
}

bool WorkerThread::get_job(Job*& job)
{
    // First, try to pop a job from the private queue, then the public queue, only then try to steal
    // Logical or is short-circuiting, only one job will be popped
    ANNOTATE_HAPPENS_AFTER(&queues_[Q_PRIVATE]); // Avoid false positives with TSan
    ANNOTATE_HAPPENS_AFTER(&queues_[Q_PUBLIC]);  // Avoid false positives with TSan
    return (queues_[Q_PRIVATE].try_pop(job) || queues_[Q_PUBLIC].try_pop(job) || steal_job(job));
}

bool WorkerThread::steal_job(Job*& job)
{
    for (size_t jj = 0; jj < props_.max_stealing_attempts; ++jj)
    {
        auto& worker = js_->get_worker(rr_next());
        ANNOTATE_HAPPENS_AFTER(&worker.queues_[Q_PUBLIC]); // Avoid false positives with TSan
        if (worker.queues_[Q_PUBLIC].try_pop(job))
        {
#ifdef K_USE_JOB_SYSTEM_PROFILING
            ++activity_.stolen;
#endif
            return true;
        }
    }
    return false;
}

void WorkerThread::process(Job* job)
{
#ifdef K_USE_JOB_SYSTEM_PROFILING
    auto start = std::chrono::high_resolution_clock::now();
#endif

    job->kernel();

#ifdef K_USE_JOB_SYSTEM_PROFILING
    auto stop = std::chrono::high_resolution_clock::now();
    auto start_us = std::chrono::time_point_cast<std::chrono::microseconds>(start).time_since_epoch().count();
    auto stop_us = std::chrono::time_point_cast<std::chrono::microseconds>(stop).time_since_epoch().count();

    activity_.active_time_us += stop_us - start_us;
    ++activity_.executed;

    // If an instrumentation session exists, push profile for this job
    if (auto* instr = js_->get_instrumentation_session())
    {
        ProfileResult result;
        result.name = job->meta.name;
        result.category = "task";
        result.start = start_us;
        result.end = stop_us;
        result.thread_id = js_->this_thread_id();
        instr->push(result);
    }
#endif

    JobState expected = JobState::Executing;
    bool success = job->exchange_state(expected, JobState::Processed);
    K_ASSERT(success, "Failed to mark job as processed.", js_->log_channel_);

    schedule_children(job);

    if (job->barrier_id != k_no_barrier)
    {
        js_->get_barrier(job->barrier_id).remove_dependency();
    }

    if (!job->keep_alive)
    {
        js_->release_job(job);
    }

    ss_->pending.fetch_sub(1, std::memory_order_release);
}

void WorkerThread::schedule_children(Job* job)
{
    for (Job* child : *job)
    {
        child->remove_dependency();

        /*
            If two parents finish at the same time, they could potentially schedule the
            same children at the same time. The mark_scheduled() call makes sure that only
            one parent will get to schedule the children jobs.
        */
        if (child->is_ready() && js_->try_schedule(child, 0))
        {
#ifdef K_USE_JOB_SYSTEM_PROFILING
            ++activity_.scheduled;
#endif
        }
    }
}

bool WorkerThread::foreground_work()
{
    K_ASSERT(!is_background(), "foreground_work() should not be called in a background thread.", nullptr);
    Job* job = nullptr;
    JobState expected = JobState::Pending;
    if (get_job(job) && job->exchange_state(expected, JobState::Executing))
    {
        process(job);
        return true;
    }

    return false;
}

void WorkerThread::panic()
{
    Job* job = nullptr;
    while (queues_[Q_PRIVATE].try_pop(job))
    {
        if (job->meta.is_essential())
        {
            job->kernel();
        }
    }
}

} // namespace kb::th