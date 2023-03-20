#pragma once

#include "../formatter.h"

namespace kb::log
{

class PowerlineTerminalFormatter : public Formatter
{
public:
    void print(const LogEntry &, const ChannelPresentation &) override;
};

} // namespace kb::log