#include "thread/job/impl/monitor.h"
#include "logger2/logger.h"
#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job_system.h"
#include "time/instrumentation.h"

#include <fstream>

namespace kb
{
namespace th
{

Monitor::Monitor(JobSystem &js) : js_(js)
{
}

void Monitor::update_statistics()
{
    WorkerActivity activity;
    while (pop_thread_activity(activity))
    {
        auto tid = activity.tid;
        stats_[tid].active_time_ms += double(activity.active_time_us) / 1000.0;
        stats_[tid].idle_time_ms += double(activity.idle_time_us) / 1000.0;
        stats_[tid].total_executed += activity.executed;
        stats_[tid].total_stolen += activity.stolen;
        stats_[tid].total_scheduled += activity.scheduled;
        ++stats_[tid].cycles;
    }
}

void Monitor::log_statistics(tid_t tid, const kb::log::Channel *channel) const
{
    K_ASSERT(tid < js_.get_threads_count(), "Worker TID out of range.");

    const auto &stats = get_statistics(tid);
    double mean_active_ms = stats.active_time_ms / double(stats.cycles);
    double mean_idle_ms = stats.idle_time_ms / double(stats.cycles);
    double mean_activity = 100.0 * mean_active_ms / (mean_idle_ms + mean_active_ms);
    float jobs_per_cycle = float(stats.total_executed) / float(stats.cycles);

    klog(channel).uid("Monitor").debug(R"(Thread #{}
Sleep cycles:         {}
Mean active time:     {}ms
Mean idle time:       {}ms
Mean activity ratio:  {}%
Total executed:       {} jobs
Total stolen:         {} jobs
Total scheduled:      {} jobs
Average jobs / cycle: {})",
                                        tid, stats.cycles, mean_active_ms, mean_idle_ms, mean_activity,
                                        stats.total_executed, stats.total_stolen, stats.total_scheduled,
                                        jobs_per_cycle);
}

} // namespace th
} // namespace kb