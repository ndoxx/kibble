#pragma once

#include "../formatter.h"

namespace kb::log
{

class VSCodeTerminalFormatter : public Formatter
{
public:
    void print(const LogEntry &, const ChannelPresentation &) override;
};

} // namespace kb::log