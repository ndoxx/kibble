#include "channel.h"
#include "entry.h"
#include <fmt/color.h>
#include <fmt/core.h>

namespace kb::log
{

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

void Channel::submit(struct LogEntry &entry)
{
    // Check if the severity level is high enough
    if (entry.severity > level_)
        return;

    // Check compliance with policies
    for (const auto &ppol : policies_)
        if (!ppol->transform_filter(entry))
            return;

    // Send to all attached sinks
    for (auto &psink : sinks_)
        psink->submit(entry, presentation_);
}

} // namespace kb::log