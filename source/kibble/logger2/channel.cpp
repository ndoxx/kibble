#include "channel.h"
#include "entry.h"
#include "thread/job/job_system.h"
#include <fmt/color.h>
#include <fmt/core.h>

namespace kb::log
{

th::JobSystem *Channel::s_js_ = nullptr;
uint32_t Channel::s_worker_ = 1;
bool Channel::s_exit_on_fatal_error_ = true;

inline auto to_rgb(math::argb32_t color)
{
    return fmt::rgb{uint8_t(color.r()), uint8_t(color.g()), uint8_t(color.b())};
}

inline std::string create_channel_tag(const std::string &short_name, math::argb32_t color)
{
    return fmt::format(
        "{}", fmt::styled(short_name, fmt::bg(to_rgb(color)) | fmt::fg(fmt::color::white) | fmt::emphasis::bold));
}

Channel::Channel(Severity level, const std::string &full_name, const std::string &short_name, math::argb32_t tag_color)
    : presentation_{full_name, create_channel_tag(short_name, tag_color)}, level_(level)
{
}

void Channel::attach_sink(std::shared_ptr<Sink> psink)
{
    sinks_.push_back(psink);
    sinks_.back()->on_attach();
}

void Channel::attach_policy(std::shared_ptr<Policy> ppolicy)
{
    policies_.push_back(ppolicy);
}

void Channel::submit(LogEntry &&entry) const
{
    // Check if the severity level is high enough
    if (entry.severity > level_)
        return;

    bool fatal = entry.severity == Severity::Fatal;

    // Check compliance with policies
    for (const auto &ppol : policies_)
        if (!ppol->transform_filter(entry))
            return;

    // Send to all attached sinks
    if (s_js_ == nullptr)
    {
        for (auto &psink : sinks_)
            psink->submit_lock(entry, presentation_);
    }
    else
    {
        // Set thread id
        entry.thread_id = s_js_->this_thread_id();
        // Schedule logging task. Log entry is moved.
        s_js_
            ->create_task(
                [this, entry = std::move(entry)]() {
                    for (auto &psink : sinks_)
                        psink->submit(entry, presentation_);
                },
                th::JobMetadata(th::force_worker(s_worker_), "Log"))
            .schedule();
    }

    if (s_exit_on_fatal_error_ && fatal)
    {
        if (s_js_)
            s_js_->shutdown();

        exit(0);
    }
}

} // namespace kb::log