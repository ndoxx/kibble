#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job.h"

#include <algorithm>

namespace kb
{
namespace th
{

Scheduler::Scheduler(JobSystem& js) : js_(js) { }

RoundRobinScheduler::RoundRobinScheduler(JobSystem& js) : Scheduler(js) {}

void RoundRobinScheduler::schedule(Job* job)
{
    if(js_.get_scheme().enable_foreground_work &&
       (job->metadata.execution_policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    js_.get_worker(round_robin_)->submit(job);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
}

MininmumLoadScheduler::MininmumLoadScheduler(JobSystem& js) : Scheduler(js) {}

void MininmumLoadScheduler::schedule(Job* job)
{
    if(job->metadata.label != 0)
    {
        const auto& job_size = js_.get_monitor().get_job_size();
        auto findit = job_size.find(job->metadata.label);
        if(findit != job_size.end())
        {
            // Constrain possible TIDs set if foreground work is enabled and job is async
            size_t offset = 0;
            if(js_.get_scheme().enable_foreground_work &&
               job->metadata.execution_policy == SchedulerExecutionPolicy::async)
                offset = 1;

            // Find worker with minimal load and assign it the job
            const auto& load = js_.get_monitor().get_load();
            auto min_load_it = std::min_element(load.begin() + offset, load.begin() + js_.get_threads_count());
            size_t min_load_idx = size_t(std::distance(load.begin(), min_load_it));
            js_.get_monitor().add_load(min_load_idx, findit->second);
            js_.get_worker(min_load_idx)->submit(job);
            return;
        }
    }

    // Fallback to round-robin selection
    if(js_.get_scheme().enable_foreground_work &&
       (job->metadata.execution_policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    js_.get_worker(round_robin_)->submit(job);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
}

} // namespace th
} // namespace kb