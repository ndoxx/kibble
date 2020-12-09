#include "thread/job/impl/worker.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/job_system.h"
#include "time/clock.h"

namespace kb
{
namespace th
{

WorkerThread::WorkerThread(const WorkerDescriptor& desc, JobSystem& jobsys)
    : tid_(desc.tid), can_steal_(desc.can_steal), is_background_(desc.is_background), js_(jobsys),
      ss_(js_.get_shared_state()), dist_(0, js_.get_threads_count() - 1)
{
    (void)tid_;
#if PROFILING
    activity_.tid = tid_;
#endif
}

void WorkerThread::spawn()
{
    if(is_background_)
        thread_ = std::thread(&WorkerThread::run, this);
}

void WorkerThread::join()
{
    if(is_background_)
        thread_.join();
}

void WorkerThread::submit(Job* job)
{
    ANNOTATE_HAPPENS_BEFORE(&jobs_); // Avoid false positives with TSan
    jobs_.push(job);
}

void WorkerThread::run()
{
    K_ASSERT(is_background_, "run() should not be called in the main thread.");

    while(ss_.running.load(std::memory_order_relaxed))
    {
        state_.store(State::Running, std::memory_order_release);

        Job* job = nullptr;
        if(get_job(job))
        {
            process(job);
            continue;
        }

        state_.store(State::Idle, std::memory_order_release);
#if PROFILING
        microClock clk;
#endif
        std::unique_lock<std::mutex> lock(ss_.wake_mutex);
        // The first condition in passed lambda avoids a possible deadlock, where a worker can
        // go to sleep with a non-empty queue and never wakes up, while the main thread waits for
        // the pending jobs it holds.
        // The second condition forces workers to wake up when the job system shuts down.
        // This avoids another deadlock on exit.
        ss_.cv_wake.wait(lock, [this]() { return !jobs_.was_empty() || !ss_.running.load(std::memory_order_relaxed); });
#if PROFILING
        activity_.idle_time_us += clk.get_elapsed_time().count();
        js_.get_monitor().report_thread_activity(activity_);
        activity_.reset();
#endif
    }

    state_.store(State::Stopping, std::memory_order_release);
}

bool WorkerThread::get_job(Job*& job)
{
    // First, try to pop a job from the queue
    ANNOTATE_HAPPENS_AFTER(&jobs_); // Avoid false positives with TSan
    bool has_job = jobs_.try_pop(job);

    // If queue is empty, try to steal a job
    if(!has_job && can_steal_)
    {
        // TODO: random shuffle
        const auto& workers = js_.get_workers();
        for(size_t ii = 0; ii < workers.size() && has_job == false; ++ii)
        {
            if(ii == tid_)
                continue;
            auto& worker = *workers[ii];
            ANNOTATE_HAPPENS_AFTER(&worker.jobs_); // Avoid false positives with TSan
            has_job = worker.jobs_.try_pop(job);
        }
        /*auto& worker = random_worker();
        ANNOTATE_HAPPENS_AFTER(&worker.jobs_); // Avoid false positives with TSan
        has_job = worker.jobs_.try_pop(job);*/
#if PROFILING
        if(has_job)
            ++activity_.stolen;
#endif
    }

    return has_job;
}

void WorkerThread::process(Job* job)
{
    microClock clk;
    job->kernel();
    job->meta.execution_time_us = clk.get_elapsed_time().count();
    job->finished.store(true, std::memory_order_release);
    ss_.pending.fetch_sub(1);
#if PROFILING
    activity_.active_time_us += clk.get_elapsed_time().count();
    ++activity_.executed;
#endif
}

bool WorkerThread::foreground_work()
{
    K_ASSERT(!is_background_, "foreground_work() should not be called in a background thread.");
    Job* job = nullptr;
    if(jobs_.try_pop(job))
    {
        process(job);
        return true;
    }
    return false;
}

WorkerThread& WorkerThread::random_worker()
{
    size_t idx = dist_(rd_);
    while(idx == tid_)
        idx = dist_(rd_);

    return js_.get_worker(idx);
}

} // namespace th
} // namespace kb