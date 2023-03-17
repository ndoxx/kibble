#pragma once
#include <string>

namespace kb::log
{

struct LogEntry;
struct ChannelPresentation;

class Formatter
{
public:
    virtual ~Formatter() = default;
    virtual std::string format_string(const LogEntry &, const ChannelPresentation &) { return ""; }
    virtual void print(const LogEntry &, const ChannelPresentation &) {}
};

} // namespace kb::log