#include "thread/job.h"
#include "thread/impl/worker.h"
#include "time/clock.h"

namespace kb
{

WorkerThread::WorkerThread(uint32_t tid, bool background, bool can_steal, JobSystem& js)
    : tid_(tid), background_(background), can_steal_(can_steal), js_(js), ss_(*js.ss_), dist_(0, ss_.workers_count - 1)
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
    dead_jobs_.push(job);
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
            ss_.cv_wake.wait(lck); // Spurious wakeups have no effect because we check if the queue is empty
#if PROFILING
            idle_time_us_ += clk.get_elapsed_time().count();
#endif
        }
    }

    state_.store(State::Stopping, std::memory_order_release);
}

void WorkerThread::foreground_work()
{
    K_ASSERT(!background_, "foreground_work() should not be called in a background thread.");
    Job* job = nullptr;
    if(jobs_.try_pop(job))
        execute(job);
}

} // namespace kb