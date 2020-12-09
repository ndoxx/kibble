#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job_system.h"

#include <algorithm>

namespace kb
{
namespace th
{

Scheduler::Scheduler(JobSystem& js) : js_(js) {}

RoundRobinScheduler::RoundRobinScheduler(JobSystem& js) : Scheduler(js) {}

void RoundRobinScheduler::dispatch(Job* job)
{
    worker_affinity_t next_worker_mask = 1 << round_robin_;
    while((job->meta.worker_affinity & next_worker_mask) == 0)
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    js_.get_worker(round_robin_).submit(job);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
}

MininmumLoadScheduler::MininmumLoadScheduler(JobSystem& js) : Scheduler(js) {}

void MininmumLoadScheduler::dispatch(Job* job)
{
    // If affinity is specified, find worker
    if(job->meta.worker_affinity != WORKER_AFFINITY_ANY)
    {
        size_t idx = 0;
        while((job->meta.worker_affinity & (1 << idx)) == 0)
            ++idx;
        K_ASSERT(idx < js_.get_threads_count(), "Bad worker affinity.");
        js_.get_worker(idx).submit(job);
    }

    if(job->meta.label != 0)
    {
        const auto& job_size = js_.get_monitor().get_job_size();
        auto findit = job_size.find(job->meta.label);
        if(findit != job_size.end())
        {
            // Find worker with minimal load and assign it the job
            const auto& load = js_.get_monitor().get_load();
            auto min_load_it = std::min_element(load.begin(), load.begin() + js_.get_threads_count());
            size_t min_load_idx = size_t(std::distance(load.begin(), min_load_it));
            js_.get_monitor().add_load(min_load_idx, findit->second);
            js_.get_worker(min_load_idx).submit(job);
            return;
        }
    }

    // Fallback to round-robin selection
    js_.get_worker(round_robin_).submit(job);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
}

} // namespace th
} // namespace kb