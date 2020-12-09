#include "thread/job2/impl/worker.h"
#include "thread/job2/job_system.h"

namespace kb
{
namespace th2
{

WorkerThread::WorkerThread(const WorkerDescriptor& desc, JobSystem& jobsys)
    : tid_(desc.tid), can_steal_(desc.can_steal), is_background_(desc.is_background), js_(jobsys),
      ss_(js_.get_shared_state())
{
    (void)tid_;
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

void WorkerThread::run()
{
    while(ss_.running.load(std::memory_order_relaxed))
    {
        Job* job = nullptr;
        std::unique_lock<std::mutex> lock(ss_.wake_mutex);
        if(!get_job(job))
        {
            // The first condition in passed lambda avoids a possible deadlock, where a worker can
            // go to sleep with a non-empty queue and never wakes up, while the main thread waits for
            // the pending jobs it holds.
            // The second condition forces workers to wake up when the job system shuts down.
            // This avoids another deadlock on exit.
            state_.store(State::Idle, std::memory_order_release);
            ss_.cv_wake.wait(lock,
                             [this]() { return !jobs_.was_empty() || !ss_.running.load(std::memory_order_relaxed); });
        }

        // Execute job
        if(job)
        {
            state_.store(State::Running, std::memory_order_release);
            job->kernel();
            ss_.pending.fetch_sub(1);
        }
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
    }

    return has_job;
}

} // namespace th2
} // namespace kb