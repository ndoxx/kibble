#pragma once

#include <array>
#include <filesystem>
#include <map>

#include "thread/job/impl/common.h"

namespace fs = std::filesystem;

namespace kb
{
namespace th
{

struct WorkerStats
{
    double active_time_ms = 0.0;
    double idle_time_ms = 0.0;
    unsigned long long total_executed = 0;
    unsigned long long total_stolen = 0;
    unsigned long long total_resubmit = 0;
    size_t cycles = 0;
};

class JobSystem;
struct JobMetadata;
class Monitor
{
public:
    Monitor(JobSystem& js);

    // Export a file containing monitoring information for labeled jobs
    void export_job_profiles(const fs::path& filepath);
    // Load a job profile information file
    void load_job_profiles(const fs::path& filepath);
    // Call after a job has been executed to report its execution profile
    void report_job_execution(const JobMetadata&);
    // Process all worker activity reports in the queue
    void update_statistics();
    // Show a worker's statistics
    void log_statistics(tid_t tid) const;
    // Reset workers load info
    void wrap();

    inline const auto& get_job_size() const { return job_size_; }
    inline const auto& get_load() const { return load_; }
    inline const auto& get_statistics(tid_t tid) const { return stats_[tid]; }
    inline void add_load(size_t idx, int64_t job_size) { load_[idx] += job_size; }

    // Called by workers when they wake up to submit their activity reports
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

} // namespace th
} // namespace kb