#pragma once

#include "../sink.h"

namespace kb::log
{

class ConsoleSink : public Sink
{
public:
    ~ConsoleSink() = default;
    void submit(const struct LogEntry &, const struct ChannelPresentation &) override;
};

} // namespace kb::log