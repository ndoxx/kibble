#include "thread/job.h"
#include "thread/impl/scheduler.h"
#include "thread/impl/worker.h"

#include <algorithm>

namespace kb
{

Scheduler::Scheduler(JobSystem& js) : js_(js) {}

RoundRobinScheduler::RoundRobinScheduler(JobSystem& js) : Scheduler(js) {}

WorkerThread* RoundRobinScheduler::select(uint64_t, SchedulerExecutionPolicy policy)
{
    if(js_.get_scheme().enable_foreground_work && (policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    auto* worker = js_.get_worker(round_robin_);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
    return worker;
}

AssociativeDynamicScheduler::AssociativeDynamicScheduler(JobSystem& js) : Scheduler(js)
{
    loads_.resize(js.get_threads_count());
    std::fill(loads_.begin(), loads_.end(), 0);
}

WorkerThread* AssociativeDynamicScheduler::select(uint64_t label, SchedulerExecutionPolicy policy)
{
    if(label != 0)
    {
        auto findit = job_durations_.find(label);
        if(findit != job_durations_.end())
        {
            // Find worker with minimal load and assign it the job
            auto min_load_it = std::min_element(loads_.begin(), loads_.end());
            size_t min_load_idx = size_t(std::distance(loads_.begin(), min_load_it));

            if(js_.get_scheme().enable_foreground_work && (policy == SchedulerExecutionPolicy::async) &&
               (min_load_idx == 0))
                min_load_it = std::min_element(loads_.begin() + 1, loads_.end());

            *min_load_it += findit->second;
            return js_.get_worker(min_load_idx);
        }
    }

    // Fallback to round-robin selection
    if(js_.get_scheme().enable_foreground_work && (policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    auto* worker = js_.get_worker(round_robin_);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
    return worker;
}

void AssociativeDynamicScheduler::report(const JobMetadata& meta)
{
    if(meta.label == 0)
        return;

    // Update execution time associated to this label using a moving average
    auto findit = job_durations_.find(meta.label);
    if(findit == job_durations_.end())
        job_durations_.insert({meta.label, meta.execution_time_us});
    else
        findit->second = (findit->second + meta.execution_time_us) / 2;
}

void AssociativeDynamicScheduler::reset() { std::fill(loads_.begin(), loads_.end(), 0); }

} // namespace kb