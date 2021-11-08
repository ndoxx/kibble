#include "thread/job/impl/monitor.h"
#include "thread/job/impl/scheduler.h"
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
}

void RoundRobinScheduler::dispatch(Job *job)
{
    while ((job->meta.worker_affinity & (1 << round_robin_)) == 0)
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    js_.get_worker(round_robin_).submit(job);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
}

MinimumLoadScheduler::MinimumLoadScheduler(JobSystem &js) : Scheduler(js)
{
}

void MinimumLoadScheduler::dispatch(Job *job)
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
            const auto &load = js_.get_monitor().get_load();
            long min_load = std::numeric_limits<long>::max();
            size_t min_load_idx = 0;
            for (size_t ii = 0; ii < js_.get_threads_count(); ++ii)
            {
                if ((job->meta.worker_affinity & (1 << ii)) != 0 && load[ii] < min_load)
                {
                    min_load = load[ii];
                    min_load_idx = ii;
                }
            }
            js_.get_monitor().add_load(min_load_idx, findit->second);
            js_.get_worker(min_load_idx).submit(job);
            return;
        }
    }

    // Fallback to round-robin selection
    while ((job->meta.worker_affinity & (1 << round_robin_)) == 0)
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    js_.get_worker(round_robin_).submit(job);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
}

} // namespace th
} // namespace kb