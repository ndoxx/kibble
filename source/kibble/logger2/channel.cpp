#include "channel.h"
#include "entry.h"

namespace kb::log
{

Channel::Channel(Severity level, const ChannelPresentation &presentation) : presentation_{presentation}, level_(level)
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