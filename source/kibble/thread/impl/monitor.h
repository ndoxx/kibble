#pragma once

#include <map>
#include <array>

#include "thread/impl/common.h"

namespace kb
{

struct WorkerStats
{
	double active_time_ms = 0.0;
	double idle_time_ms = 0.0;
	unsigned long long total_executed = 0;
	unsigned long long total_stolen = 0;
	size_t cycles = 0;
};

class JobSystem;
struct JobMetadata;
class Monitor
{
public:
    Monitor(JobSystem& js);
    void report_job_execution(const JobMetadata&);
    void update_statistics();
    void log_statistics(tid_t tid) const;
    void wrap();

    inline const auto& get_job_size() const { return job_size_; }
    inline const auto& get_load() const { return load_; }
    inline const auto& get_statistics(tid_t tid) const { return stats_[tid]; }
    inline void add_load(size_t idx, int64_t job_size) { load_[idx] += job_size; }
    inline void report_thread_activity(WorkerActivity activity)
    {
        ANNOTATE_HAPPENS_BEFORE(&activity_queue_); // Avoid false positives with TSan
        activity_queue_.push(std::move(activity));
    }

private:
    inline bool pop_thread_activity(WorkerActivity& activity)
    {
        ANNOTATE_HAPPENS_AFTER(&activity_queue_); // Avoid false positives with TSan
        return activity_queue_.try_pop(activity);
    }

private:
    std::map<uint64_t, int64_t> job_size_;
    std::array<int64_t, k_max_threads> load_;
    std::array<WorkerStats, k_max_threads> stats_;
    JobSystem& js_;
    ActivityQueue<WorkerActivity> activity_queue_;
};

} // namespace kb