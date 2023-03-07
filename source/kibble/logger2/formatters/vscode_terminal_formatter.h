#pragma once

#include "../formatter.h"

namespace kb::log
{

class VSCodeTerminalFormatter : public Formatter
{
public:
    std::string format_string(const LogEntry &, const ChannelPresentation &) override;
    void print(const LogEntry &, const ChannelPresentation &) override;
};

} // namespace kb::log