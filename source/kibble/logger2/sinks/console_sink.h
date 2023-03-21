#pragma once

#include "../sink.h"

namespace kb::log
{

/**
 * @brief Direct all input log entries to the terminal
 *
 * The formatter will decide how exactly the logs are styled and displayed
 *
 */
class ConsoleSink : public Sink
{
public:
    ~ConsoleSink() = default;

    void submit(const struct LogEntry &, const struct ChannelPresentation &) override;
};

} // namespace kb::log