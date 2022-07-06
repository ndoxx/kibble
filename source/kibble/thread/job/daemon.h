#pragma once

#include "thread/job/job_system.h"
#include <cstdint>
#include <map>

namespace kb
{
namespace th
{

struct SchedulingData
{
    int64_t interval_ms = 0;
    int64_t cooldown_ms = 0;
    int64_t ttl = 0;
    bool marked_for_deletion = false;
};

/**
 * @brief Recurring job
 *
 */
struct Daemon
{
    SchedulingData scheduling_data;
    Job *job = nullptr;
};

using DaemonHandle = size_t;

class DaemonScheduler
{
public:
    DaemonScheduler(JobSystem &js);
    ~DaemonScheduler();

    DaemonHandle create(JobKernel &&kernel, const SchedulingData &scheduling_data,
                        const JobMetadata &meta = JobMetadata{});

    void kill(DaemonHandle hnd);

    void update(int64_t delta_t_ms);

private:
    JobSystem &js_;
    DaemonHandle current_handle_ = 0;
    std::map<DaemonHandle, Daemon> daemons_;
    std::vector<DaemonHandle> kill_list_;
};

} // namespace th
} // namespace kb