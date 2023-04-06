#pragma once

#include "thread/job/job_system.h"
#include "time/clock.h"
#include <cstdint>
#include <map>

namespace kb
{
namespace th
{

/**
 * @brief Data necessary for repeated scheduling of daemons.
 *
 */
struct SchedulingData
{
    /// The interval at which the daemon will be rescheduled
    float interval_ms = 0;
    /// Daemon rescheduled when this reaches zero, can be used as an initial execution delay
    float cooldown_ms = 0;
    /// If not set to zero, controls how many times the daemon will be rescheduled
    int64_t ttl = 0;
};

/**
 * @internal
 * @brief Recurring job
 *
 */
struct Daemon
{
    /// Data necessary for repeated scheduling of this daemon
    SchedulingData scheduling_data;
    /// When set to true, this daemon will be returned to the pool by DaemonScheduler::update()
    bool marked_for_deletion = false;
    /// The job is kept alive by the JobSystem, the DaemonScheduler needs to store this pointer
    Job *job = nullptr;
};

/// Refers to a particular daemon
using DaemonHandle = size_t;

/**
 * @brief This system can create, automatically schedule and kill recurring jobs.
 *
 * Recurring jobs are referred to as daemons. Once created, a daemon will be rescheduled
 * each time its internal cooldown counter reaches zero. For this to happen, the update()
 * function needs to be called regularly (typically each frame).
 *
 */
class DaemonScheduler
{
public:
    /**
     * @brief Construct a new DaemonScheduler.
     *
     * @param js Reference to an existing JobSystem instance.
     */
    DaemonScheduler(JobSystem &js, const kb::log::Channel *log_channel = nullptr);

    /**
     * @brief Kill all daemons and destroy the DaemonScheduler.
     *
     */
    ~DaemonScheduler();

    /**
     * @brief Create a daemon.
     *
     * A daemon is a recurring task, rescheduled regularly and automatically.
     * The underlying job will be kept alive by the JobSystem, meaning it will
     * not be released to the pool once the job finishes, so there is no data
     * copy happening each time a daemon is rescheduled.
     *
     * @param kernel Function executed by the daemon.
     * @param scheduling_data Data for the automated scheduling.
     * @param meta Job metadata.
     * @return DaemonHandle A handle to the newly created daemon.
     */
    DaemonHandle create(JobKernel &&kernel, const SchedulingData &scheduling_data,
                        const JobMetadata &meta = JobMetadata{});

    /**
     * @brief Manually stop and release a daemon.
     *
     * @param hnd Handle of the daemon to kill.
     */
    void kill(DaemonHandle hnd);

    /**
     * @brief Call regularly to force daemon rescheduling.
     *
     * An internal clock is used to measure the time interval between each call, and this
     * interval is subtracted from each daemon cooldown counter. Daemons whose counters
     * reach zero are rescheduled. If the ttl property of a daemon was initialized with
     * a non-zero value, it is also decremented each time the daemon task is rescheduled,
     * and the daemon is automatically killed once its ttl reaches zero.
     *
     */
    void update();

private:
    JobSystem &js_;
    std::map<DaemonHandle, Daemon> daemons_;
    std::vector<DaemonHandle> kill_list_;
    microClock clock_;
    DaemonHandle current_handle_ = 0;
    float delta_t_ms_ = 0.f;
    const kb::log::Channel *log_channel_ = nullptr;
};

} // namespace th
} // namespace kb