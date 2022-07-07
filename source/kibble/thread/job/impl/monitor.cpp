#include "thread/job/impl/monitor.h"
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

void Monitor::log_statistics(tid_t tid) const
{
    K_ASSERT(tid < js_.get_threads_count(), "Worker TID out of range.");

    const auto &stats = get_statistics(tid);
    double mean_active_ms = stats.active_time_ms / double(stats.cycles);
    double mean_idle_ms = stats.idle_time_ms / double(stats.cycles);
    double mean_activity = 100.0 * mean_active_ms / (mean_idle_ms + mean_active_ms);
    float jobs_per_cycle = float(stats.total_executed) / float(stats.cycles);

    // clang-format off
    KLOG("thread", 1) << KS_INST_ << "Thread #" << tid << std::endl;
    KLOGI << "Sleep cycles:         " << stats.cycles << std::endl;
    KLOGI << "Mean active time:     " << mean_active_ms << "ms" << std::endl;
    KLOGI << "Mean idle time:       " << mean_idle_ms << "ms" << std::endl;
    KLOGI << "Mean activity ratio:  " << mean_activity << '%' << std::endl;
    KLOGI << "Total executed:       " << stats.total_executed << " job" << ((stats.total_executed > 1) ? "s" : "") << std::endl;
    KLOGI << "Total stolen:         " << stats.total_stolen << " job" << ((stats.total_stolen > 1) ? "s" : "") << std::endl;
    KLOGI << "Total scheduled:      " << stats.total_scheduled << " job" << ((stats.total_scheduled > 1) ? "s" : "") << std::endl;
    KLOGI << "Average jobs / cycle: " << jobs_per_cycle << " job" << ((jobs_per_cycle > 1.f) ? "s" : "") << std::endl;
    // clang-format on
}

} // namespace th
} // namespace kb