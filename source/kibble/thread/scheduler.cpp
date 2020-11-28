#include "thread/scheduler.h"
#include "thread/job.h"

namespace kb
{

Scheduler::Scheduler(JobSystem& js) : js_(js) {}

RoundRobinScheduler::RoundRobinScheduler(JobSystem& js) : Scheduler(js) {}

WorkerThread* RoundRobinScheduler::select(SchedulerExecutionPolicy policy)
{
    if(js_.get_scheme().enable_foreground_work && (policy == SchedulerExecutionPolicy::async) && (round_robin_ == 0))
        round_robin_ = (round_robin_ + 1) % js_.get_threads_count();

    auto* worker = js_.get_worker(round_robin_);
    round_robin_ = (round_robin_ + 1) % js_.get_threads_count();
    return worker;
}

} // namespace kb