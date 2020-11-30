#include "thread/impl/monitor.h"
#include "thread/impl/worker.h"
#include "thread/job.h"

namespace kb
{

Monitor::Monitor(JobSystem& js) : js_(js)
{
	(void)js_;
    wrap();
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

void Monitor::update_statistics()
{
	WorkerActivity activity;
	while(pop_thread_activity(activity))
	{
		auto tid = activity.tid;
		stats_[tid].active_time_ms += double(activity.active_time_us) / 1000.0;
		stats_[tid].idle_time_ms += double(activity.idle_time_us) / 1000.0;
		stats_[tid].total_executed += activity.executed;
		stats_[tid].total_stolen += activity.stolen;
		++stats_[tid].cycles;
	}
}

void Monitor::log_statistics(tid_t tid) const
{
    const auto& stats = get_statistics(tid);
    double mean_active_ms = stats.active_time_ms / double(stats.cycles);
    double mean_idle_ms = stats.idle_time_ms / double(stats.cycles);
    double mean_activity = 100.0 * mean_active_ms / (mean_idle_ms + mean_active_ms);
    float jobs_per_cycle = float(stats.total_executed) / float(stats.cycles);
    
    KLOG("thread", 1) << WCC('i') << "Thread #" << tid << std::endl;
    KLOGI << "Mean active time:     " << mean_active_ms << "ms" << std::endl;
    KLOGI << "Mean idle time:       " << mean_idle_ms << "ms" << std::endl;
    KLOGI << "Mean activity ratio:  " << mean_activity << '%' << std::endl;
    KLOGI << "Total executed:       " << stats.total_executed << " job" << ((stats.total_executed > 1) ? "s" : "") << std::endl;
    KLOGI << "Total stolen:         " << stats.total_stolen << " job" << ((stats.total_stolen > 1) ? "s" : "") << std::endl;
    KLOGI << "Average jobs / cycle: " << jobs_per_cycle << " job" << ((jobs_per_cycle > 1.f) ? "s" : "") << std::endl;
}

} // namespace kb