#include "thread/impl/scheduler.h"
#include "thread/impl/monitor.h"
#include "thread/impl/worker.h"
#include "thread/job.h"

#include <algorithm>

namespace kb
{

Scheduler::Scheduler(JobSystem& js) : js_(js) { scheduled_jobs_.reserve(k_max_jobs * k_max_threads); }

void Scheduler::schedule(Job* job) { scheduled_jobs_.push_back(job); }

RoundRobinScheduler::RoundRobinScheduler(JobSystem& js) : Scheduler(js) {}

void RoundRobinScheduler::submit()
{
    for(auto* job : scheduled_jobs_)
    {
        if(js_.get_scheme().enable_foreground_work &&
           (job->metadata.execution_policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
            round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

        js_.get_worker(round_robin_)->submit(job);
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
    }
    scheduled_jobs_.clear();
}

MininmumLoadScheduler::MininmumLoadScheduler(JobSystem& js) : Scheduler(js) {}

void MininmumLoadScheduler::submit()
{
    for(auto* job : scheduled_jobs_)
    {
        if(job->metadata.label != 0)
        {
            const auto& job_size = js_.get_monitor().get_job_size();
            auto findit = job_size.find(job->metadata.label);
            if(findit != job_size.end())
            {
                // Find worker with minimal load and assign it the job
                const auto& load = js_.get_monitor().get_load();
                auto min_load_it = std::min_element(load.begin(), load.end());
                size_t min_load_idx = size_t(std::distance(load.begin(), min_load_it));

                if(js_.get_scheme().enable_foreground_work &&
                   (job->metadata.execution_policy == SchedulerExecutionPolicy::async) && (min_load_idx == 0))
                    min_load_it = std::min_element(load.begin() + 1, load.end());

                js_.get_monitor().add_load(min_load_idx, findit->second);
                js_.get_worker(min_load_idx)->submit(job);
                continue;
            }
        }

        // Fallback to round-robin selection
        if(js_.get_scheme().enable_foreground_work &&
           (job->metadata.execution_policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
            round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

        js_.get_worker(round_robin_)->submit(job);
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
    }
    scheduled_jobs_.clear();
}

} // namespace kb