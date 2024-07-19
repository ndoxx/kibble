#include "daemon.h"
#include "../../assert/assert.h"
#include "../../logger2/logger.h"
#include "../../time/instrumentation.h"
#include "impl/common.h"
#include "thread/job/impl/job.h"
#include "thread/job/impl/worker.h"

namespace kb
{
namespace th
{

// clang-format off
std::string what(const std::exception_ptr &eptr = std::current_exception())
{
    if (!eptr) { throw std::bad_exception(); }

    try { std::rethrow_exception(eptr); }
    catch (const std::exception &e) { return e.what()   ; }
    catch (const std::string    &e) { return e          ; }
    catch (const char           *e) { return e          ; }
    catch (...)                     { return "unknown exception"; }
}
// clang-format on

DaemonScheduler::DaemonScheduler(JobSystem& js, const kb::log::Channel* log_channel)
    : js_(js), log_channel_(log_channel)
{
}

DaemonScheduler::~DaemonScheduler()
{
    for (auto&& [hnd, daemon] : daemons_)
    {
        daemon->job->force_state(JobState::Processed);
        js_.release_job(daemon->job);
    }
}

DaemonHandle DaemonScheduler::create(std::function<bool()> kernel, SchedulingData&& scheduling_data, JobMetadata&& meta)
{
    JS_PROFILE_FUNCTION(js_.get_instrumentation_session(), 0);

    auto&& [it, inserted] = daemons_.insert({current_handle_++, std::make_unique<Daemon>()});
    K_ASSERT(inserted, "Could not insert new daemon");

    auto& daemon = *it->second;
    daemon.scheduling_data = std::move(scheduling_data);
    daemon.job = js_.create_job(
        [this, &daemon, kernel = std::move(kernel)]() {
            bool self_terminate = false;
            try
            {
                self_terminate = !kernel();
            }
            catch (...)
            {
                klog(log_channel_)
                    .error("Exception occurred during daemon execution.\n    -> {}\n    -> Daemon will be stopped.",
                           what());
                self_terminate = true;
            }
            if (self_terminate)
                daemon.marked_for_deletion.store(true, std::memory_order_release);
        },
        std::move(meta));
    daemon.job->keep_alive = true;

    klog(log_channel_)
        .uid("DaemonScheduler")
        .verbose(R"(New daemon:
handle:    {}
interval:  {}ms
cooldown:  {}ms
ttl:       {}
tid hint:  {}
balanced:  {}
stealable: {})",
                 it->first, daemon.scheduling_data.interval_ms, daemon.scheduling_data.cooldown_ms,
                 daemon.scheduling_data.ttl, (meta.worker_affinity & kb::th::k_tid_hint_mask),
                 bool(meta.worker_affinity >> kb::th::k_balance_bit),
                 bool(meta.worker_affinity >> kb::th::k_stealable_bit));

    return it->first;
}

void DaemonScheduler::kill(DaemonHandle hnd)
{
    JS_PROFILE_FUNCTION(js_.get_instrumentation_session(), 0);

    auto findit = daemons_.find(hnd);
    K_ASSERT(findit != daemons_.end(), "Could not find daemon {}", hnd);
    findit->second->marked_for_deletion.store(true, std::memory_order_release);
}

void DaemonScheduler::update()
{
    JS_PROFILE_FUNCTION(js_.get_instrumentation_session(), 0);

    // Count time in ms since last call
    delta_t_ms_ = float(clock_.restart().count()) / 1000.f;

    // Iterate daemons, reschedule those whose cooldown reached zero
    for (auto&& [hnd, daemon] : daemons_)
    {
        SchedulingData& sd = daemon->scheduling_data;

        if (daemon->marked_for_deletion.load(std::memory_order_acquire))
        {
            // Job is not scheduled at this point, we need to manually release it
            daemon->job->force_state(JobState::Processed);
            js_.release_job(daemon->job);
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
                daemon->job->keep_alive = false;
                kill_list_.push_back(hnd);
            }

            daemon->job->reset();
            [[maybe_unused]] bool success = js_.try_schedule(daemon->job, 1);
            K_ASSERT(success, "Could not schedule job. Dameon handle: {}", hnd);
        }
    }

    // Cleanup
    for (auto hnd : kill_list_)
    {
        daemons_.erase(hnd);
        klog(log_channel_).uid("DaemonScheduler").verbose("Killed daemon {}", hnd);
    }

    kill_list_.clear();
}

} // namespace th
} // namespace kb