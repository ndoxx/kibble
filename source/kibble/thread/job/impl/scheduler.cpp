#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job_system.h"

#include <algorithm>
#include <limits>

namespace kb
{
namespace th
{

Scheduler::Scheduler(JobSystem &js) : js_(js)
{
}

RoundRobinScheduler::RoundRobinScheduler(JobSystem &js) : Scheduler(js)
{
    std::fill(round_robin_.begin(), round_robin_.end(), 0);
}

void RoundRobinScheduler::dispatch(Job *job, tid_t caller_thread)
{
    std::size_t &rr = round_robin_[caller_thread];
    while ((job->meta.worker_affinity & (1 << rr)) == 0)
        rr = (rr + 1) % js_.get_threads_count();

    js_.get_worker(rr).submit(job);
    rr = (rr + 1) % js_.get_threads_count();
}

MinimumLoadScheduler::MinimumLoadScheduler(JobSystem &js) : Scheduler(js)
{
    std::fill(round_robin_.begin(), round_robin_.end(), 0);
}

void MinimumLoadScheduler::dispatch(Job *job, tid_t caller_thread)
{
    // Create a vector of viable worker candidates based on affinity,
    // and select worker with minimal load
    if (job->meta.label != 0)
    {
        const auto &job_size = js_.get_monitor().get_job_size();
        auto findit = job_size.find(job->meta.label);
        if (findit != job_size.end())
        {
            // Find worker with minimal load and assign it the job
            int64_t min_load = std::numeric_limits<int64_t>::max();
            size_t min_load_tid = 0;
            auto compatible = js_.get_compatible_worker_ids(job->meta.worker_affinity);
            for (auto tid : compatible)
            {
                auto load = js_.get_monitor().get_load(tid);
                if (load < min_load)
                {
                    min_load = load;
                    min_load_tid = tid;
                }
            }
            js_.get_monitor().add_load(min_load_tid, findit->second);
            js_.get_worker(min_load_tid).submit(job);
            return;
        }
    }

    // Fallback to round-robin selection
    std::size_t &rr = round_robin_[caller_thread];
    while ((job->meta.worker_affinity & (1 << rr)) == 0)
        rr = (rr + 1) % js_.get_threads_count();

    js_.get_worker(rr).submit(job);
    rr = (rr + 1) % js_.get_threads_count();
}

} // namespace th
} // namespace kb