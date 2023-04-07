#include "thread/job/daemon.h"
#include "assert/assert.h"
#include "thread/job/impl/common.h"
#include "time/instrumentation.h"

namespace kb
{
namespace th
{

DaemonScheduler::DaemonScheduler(JobSystem& js, const kb::log::Channel* log_channel)
    : js_(js), log_channel_(log_channel)
{
}

DaemonScheduler::~DaemonScheduler()
{
    for (auto&& [hnd, daemon] : daemons_)
    {
        daemon.job->mark_processed();
        js_.release_job(daemon.job);
    }
}

DaemonHandle DaemonScheduler::create(JobKernel&& kernel, const SchedulingData& scheduling_data, const JobMetadata& meta)
{
    JS_PROFILE_FUNCTION(js_.get_instrumentation_session(), 0);

    auto&& [it, inserted] = daemons_.insert({current_handle_++, Daemon{}});
    K_ASSERT(inserted, "Could not insert new daemon.", log_channel_);

    auto& daemon = it->second;
    daemon.job = js_.create_job(std::forward<JobKernel>(kernel), meta);
    daemon.scheduling_data = scheduling_data;
    daemon.job->keep_alive = true;

    return it->first;
}

void DaemonScheduler::kill(DaemonHandle hnd)
{
    JS_PROFILE_FUNCTION(js_.get_instrumentation_session(), 0);

    auto findit = daemons_.find(hnd);
    K_ASSERT(findit != daemons_.end(), "Could not find daemon.", log_channel_).watch(hnd);
    findit->second.marked_for_deletion = true;
}

void DaemonScheduler::update()
{
    JS_PROFILE_FUNCTION(js_.get_instrumentation_session(), 0);

    // Count time in ms since last call
    delta_t_ms_ = float(clock_.restart().count()) / 1000.f;

    // Iterate daemons, reschedule those whose cooldown reached zero
    for (auto&& [hnd, daemon] : daemons_)
    {
        SchedulingData& sd = daemon.scheduling_data;

        if (daemon.marked_for_deletion)
        {
            // Job is not scheduled at this point, we need to manually release it
            daemon.job->mark_processed();
            js_.release_job(daemon.job);
            kill_list_.push_back(hnd);
            continue;
        }

        // Update cooldown timer
        sd.cooldown_ms -= delta_t_ms_;

        if (sd.cooldown_ms <= 0.f)
        {
            sd.cooldown_ms = sd.interval_ms;
            if (sd.ttl > 0 && --sd.ttl == 0)
            {
                daemon.job->keep_alive = false;
                kill_list_.push_back(hnd);
            }

            daemon.job->reset();
            js_.schedule(daemon.job);
        }
    }

    // Cleanup
    for (auto hnd : kill_list_)
        daemons_.erase(hnd);

    kill_list_.clear();
}

} // namespace th
} // namespace kb