#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>

#include "thread/job/impl/common.h"
#include "thread/sanitizer.h"

namespace fs = std::filesystem;

namespace kb
{

namespace th
{

/**
 * @brief Cumulated worker statistics.
 *
 */
struct WorkerStats
{
    /// Total active time in ms
    double active_time_ms = 0.0;
    /// Total idle time in ms
    double idle_time_ms = 0.0;
    /// Total number of tasks executed by the worker
    unsigned long long total_executed = 0;
    /// Total number of tasks stolen by the worker
    unsigned long long total_stolen = 0;
    /// Total number of children tasks scheduled by the worker
    unsigned long long total_scheduled = 0;
    /// Number of sleep cycles
    size_t cycles = 0;
};

class JobSystem;
struct JobMetadata;

/**
 * @brief Object responsible for gathering statistics relative to worker's activity and task execution.
 * Keeping track of such statistics can help optimize the application, and can be used by a smart load balancing
 * algorithm that estimates in advance how much time a given task takes, knowing how much time the same task took in the
 * past.
 *
 * A job profile can be exported to a custom file format and read back by the Monitor, so the load balancer can use job
 * execution statistics from a previous run.
 *
 */
class Monitor
{
public:
    /**
     * @brief Construct a new Monitor.
     *
     * @param js job system instance
     */
    Monitor(JobSystem &js);

    /**
     * @brief Process all worker activity reports in the queue.
     *
     */
    void update_statistics();

    /**
     * @brief Show a worker's statistics.
     * The "thread" logger channel must exist.
     *
     * @param tid worker id
     */
    void log_statistics(tid_t tid) const;

    /**
     * @brief Get a particular worker's statistics.
     *
     * @param tid worker id
     * @return WorkerStats struct at this worker id
     */
    inline const auto &get_statistics(tid_t tid) const
    {
        return stats_[tid];
    }

    /**
     * @brief Called by workers when they wake up to submit their activity reports.
     *
     * @param activity worker's activity report
     */
    inline void report_thread_activity(WorkerActivity activity)
    {
        ANNOTATE_HAPPENS_BEFORE(&activity_queue_); // Avoid false positives with TSan
        activity_queue_.push(std::move(activity));
    }

private:
    /**
     * @internal
     * @brief Helper used by update_statistics() to pop the next worker activity report in the queue.
     *
     * @param activity output argument
     * @return true if the activity queue is non-empty
     * @return false otherwise
     */
    inline bool pop_thread_activity(WorkerActivity &activity)
    {
        ANNOTATE_HAPPENS_AFTER(&activity_queue_); // Avoid false positives with TSan
        return activity_queue_.try_pop(activity);
    }

private:
    std::array<WorkerStats, k_max_threads> stats_;
    JobSystem &js_;
    ActivityQueue<WorkerActivity> activity_queue_;
};

} // namespace th
} // namespace kb