#include "thread/impl/worker.h"
#include "thread/job.h"
#include "time/clock.h"

namespace kb
{

WorkerThread::WorkerThread(uint32_t tid, bool background, bool can_steal, JobSystem& js)
    : tid_(tid), background_(background), can_steal_(can_steal), js_(js), ss_(*js.ss_),
      dist_(0, js_.get_threads_count() - 1)
#if PROFILING
      ,
      active_time_us_(0), idle_time_us_(0)
#endif
{
    (void)js_;
}

void WorkerThread::execute(Job* job)
{
    microClock clk;
    job->function();
    job->metadata.execution_time_us = clk.get_elapsed_time().count();
    release_handle(job->handle);
    ss_.dead_jobs.push(job);
    ss_.pending.fetch_sub(1);
#if PROFILING
    active_time_us_ += clk.get_elapsed_time().count();
#endif
}

void WorkerThread::run()
{
    K_ASSERT(background_, "run() should not be called in the main thread.");

    while(ss_.running.load(std::memory_order_acquire))
    {
        state_.store(State::Running, std::memory_order_release);

        // Get a job you lazy bastard
        Job* job = nullptr;
        ANNOTATE_HAPPENS_AFTER(&jobs_);
        if(jobs_.try_pop(job))
            execute(job);
        else
        {
            // Try to steal a job
            if(can_steal_ && try_steal(job))
            {
                execute(job);
                continue;
            }

            state_.store(State::Idle, std::memory_order_release);
#if PROFILING
            microClock clk;
#endif
            std::unique_lock<std::mutex> lck(ss_.wake_mutex);
            // The first condition in passed lambda avoids a possible deadlock, where a worker can 
            // go to sleep with a non-empty queue and never wakes up, while the main thread waits for
            // the pending jobs it holds.
            // The second condition forces workers to wake up when the job system wants to shutdown
            // and sets 'running' to false. This avoids another deadlock on exit.
            ss_.cv_wake.wait(lck, [this]() { return !jobs_.was_empty() || !ss_.running.load(std::memory_order_acquire); });
#if PROFILING
            idle_time_us_ += clk.get_elapsed_time().count();
#endif
        }
    }

    state_.store(State::Stopping, std::memory_order_release);
}

bool WorkerThread::foreground_work()
{
    K_ASSERT(!background_, "foreground_work() should not be called in a background thread.");
    Job* job = nullptr;
    if(jobs_.try_pop(job))
    {
        execute(job);
        return true;
    }
    return false;
}

WorkerThread* WorkerThread::random_worker()
{
    size_t idx = dist_(rd_);
    while(idx == tid_)
        idx = dist_(rd_);

    return js_.get_worker(idx);
}

} // namespace kb