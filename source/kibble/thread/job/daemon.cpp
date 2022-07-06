#include "thread/job/daemon.h"
#include "assert/assert.h"

namespace kb
{
namespace th
{

DaemonScheduler::DaemonScheduler(JobSystem &js) : js_(js)
{
}

DaemonScheduler::~DaemonScheduler()
{
    for (auto &&[hnd, daemon] : daemons_)
        daemon.job->keep_alive = false;
}

DaemonHandle DaemonScheduler::create(JobKernel &&kernel, const SchedulingData &scheduling_data, const JobMetadata &meta)
{
    auto &&[it, inserted] = daemons_.insert({current_handle_++, Daemon{}});
    K_ASSERT(inserted, "Could not insert new daemon.");

    auto &daemon = it->second;
    daemon.job = js_.create_job(std::forward<JobKernel>(kernel), meta);
    daemon.scheduling_data = scheduling_data;
    daemon.job->keep_alive = true;

    return it->first;
}

void DaemonScheduler::kill(DaemonHandle hnd)
{
    auto findit = daemons_.find(hnd);
    K_ASSERT(findit != daemons_.end(), "Could not find daemon.");
    findit->second.scheduling_data.marked_for_deletion = true;
}

void DaemonScheduler::update(int64_t delta_t_ms)
{
    for (auto &&[hnd, daemon] : daemons_)
    {
        SchedulingData &sd = daemon.scheduling_data;

        if (sd.marked_for_deletion)
        {
            daemon.job->keep_alive = false;
            kill_list_.push_back(hnd);
            continue;
        }

        sd.cooldown_ms -= delta_t_ms;

        if (sd.cooldown_ms <= 0)
        {
            sd.cooldown_ms = sd.interval_ms;
            if (sd.ttl > 0 && --sd.ttl == 0)
                daemon.job->keep_alive = false;

            js_.schedule(daemon.job);
        }
    }

    for (auto hnd : kill_list_)
        daemons_.erase(hnd);
}

} // namespace th
} // namespace kb