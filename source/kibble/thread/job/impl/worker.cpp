#include "kibble/thread/job/impl/worker.h"
#include "kibble/assert/assert.h"
#include "kibble/thread/job/impl/barrier.h"
#include "kibble/thread/job/impl/job.h"
#include "kibble/thread/job/impl/job_graph.h"
#include "kibble/thread/job/impl/monitor.h"
#include "kibble/thread/job/job_system.h"
#include "kibble/time/clock.h"
#include "kibble/time/instrumentation.h"
#include "kibble/util/sanitizer.h"

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <pthread.h>
#endif

namespace kb::th
{

void WorkerThread::spawn(JobSystem* js, SharedState* ss, const WorkerProperties& props)
{
    js_ = js;
    ss_ = ss;
    props_ = props;
#ifdef KB_JOB_SYSTEM_PROFILING
    activity_.tid = props_.tid;
#endif

    // Generate list of stealable workers
    // Make sure that this worker cannot steal from itself
    for (tid_t tid = 0; tid < js_->get_threads_count(); ++tid)
    {
        if (props_.tid != tid)
        {
            stealable_workers_.push_back(tid);
        }
    }

    // Spawn thread if it is not the main thread
    if (is_background())
    {
        thread_ = std::thread(&WorkerThread::run, this);
    }
}

WorkerTerminationStatus WorkerThread::terminate_and_join(std::chrono::seconds timeout)
{
    if (!is_background())
    {
        // Nothing to do for foreground thread
        return WorkerTerminationStatus::Normal;
    }

    should_terminate_.store(true, std::memory_order_release);

    // Try to acquire the wake_mutex
    {
        std::unique_lock<std::mutex> lock(ss_->wake_mutex, std::try_to_lock);
        if (lock.owns_lock())
        {
            // If we got the lock, we can notify directly
            ss_->cv_wake.notify_all();
        }
        // If we didn't get the lock, the worker might be stuck inside the critical section
    }

    // Wait for the thread to join or timeout
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout)
    {
        if (state_.load(std::memory_order_acquire) == State::Stopping)
        {
            thread_.join();
            return WorkerTerminationStatus::Normal;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Forceful termination
#ifdef _WIN32
    if (TerminateThread(thread_.native_handle(), 0))
    {
        return WorkerTerminationStatus::Forceful;
    }
#else
    if (pthread_cancel(thread_.native_handle()) == 0)
    {
        void* res;
        int s = pthread_join(thread_.native_handle(), &res);
        if (s == 0 && res == PTHREAD_CANCELED)
        {
            return WorkerTerminationStatus::Forceful;
        }
    }
#endif

    return WorkerTerminationStatus::Failed;
}

void WorkerThread::submit(Job* job, bool stealable)
{
    size_t idx = size_t(!stealable);
    ANNOTATE_HAPPENS_BEFORE(&queues_[idx]); // Avoid false positives with TSan
    queues_[idx].push(job);
}

void WorkerThread::run()
{
    K_ASSERT(is_background(), "run() should not be called in the main thread.");

    while (!should_terminate_.load(std::memory_order_acquire))
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
#ifdef KB_JOB_SYSTEM_PROFILING
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
                          [this]() { return had_pending_jobs() || should_terminate_.load(std::memory_order_acquire); });

#ifdef KB_JOB_SYSTEM_PROFILING
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
#ifdef KB_JOB_SYSTEM_PROFILING
            ++activity_.stolen;
#endif
            return true;
        }
    }
    return false;
}

void WorkerThread::process(Job* job)
{
#ifdef KB_JOB_SYSTEM_PROFILING
    auto start = std::chrono::high_resolution_clock::now();
#endif

    job->kernel();

#ifdef KB_JOB_SYSTEM_PROFILING
    auto stop = std::chrono::high_resolution_clock::now();
    auto start_us = std::chrono::time_point_cast<std::chrono::microseconds>(start).time_since_epoch().count();
    auto stop_us = std::chrono::time_point_cast<std::chrono::microseconds>(stop).time_since_epoch().count();

    activity_.active_time_us += stop_us - start_us;
    ++activity_.executed;

    // If an instrumentation session exists, push profile for this job
    if (auto* instr = js_->get_instrumentation_session())
    {
        instr->push(ProfileResult{
            .name = job->meta.name,
            .category = "task",
            .thread_id = js_->this_thread_id(),
            .start = start_us,
            .end = stop_us,
        });
    }
#endif

    JobState expected = JobState::Executing;
    [[maybe_unused]] bool success = job->exchange_state(expected, JobState::Processed);
    K_ASSERT(success, "Failed to mark job as processed.");

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
#ifdef KB_JOB_SYSTEM_PROFILING
            ++activity_.scheduled;
#endif
        }
    }
}

bool WorkerThread::foreground_work()
{
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