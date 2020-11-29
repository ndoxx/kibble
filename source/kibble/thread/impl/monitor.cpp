#include "thread/impl/monitor.h"
#include "thread/impl/worker.h"
#include "thread/job.h"

namespace kb
{

Monitor::Monitor(JobSystem& js) : js_(js)
{
    load_.resize(js_.get_threads_count());
    std::fill(load_.begin(), load_.end(), 0);
}

void Monitor::report_job_execution(const JobMetadata& meta)
{
    if(meta.label == 0)
        return;

    // Update execution time associated to this label using a moving average
    auto findit = job_size_.find(meta.label);
    if(findit == job_size_.end())
        job_size_.insert({meta.label, meta.execution_time_us});
    else
        findit->second = (findit->second + meta.execution_time_us) / 2;
}

void Monitor::wrap() { std::fill(load_.begin(), load_.end(), 0); }

} // namespace kb