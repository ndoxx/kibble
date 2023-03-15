#pragma once

#include "channel.h"
#include "entry.h"
#include "severity.h"

namespace kb::log
{

struct EntryBuilder : private LogEntry
{
public:
    EntryBuilder(int source_line, const char *source_file, const char *source_function)
        : LogEntry{.source_location = {source_line, source_file, source_function},
                   .timestamp = kb::TimeBase::timestamp()}
    {
    }

    inline EntryBuilder &chan(Channel &channel)
    {
        channel_ = &channel;
        return *this;
    }

    inline EntryBuilder &verbose(std::string_view m)
    {
        return log(Severity::Verbose, m);
    }

    inline EntryBuilder &debug(std::string_view m)
    {
        return log(Severity::Debug, m);
    }

    inline EntryBuilder &info(std::string_view m)
    {
        return log(Severity::Info, m);
    }

    inline EntryBuilder &warn(std::string_view m)
    {
        return log(Severity::Warn, m);
    }

    inline EntryBuilder &error(std::string_view m)
    {
        return log(Severity::Error, m);
    }

    inline EntryBuilder &fatal(std::string_view m)
    {
        return log(Severity::Fatal, m);
    }

private:
    EntryBuilder &log(Severity s, std::string_view m)
    {
        severity = s;
        message = m;
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

private:
    Channel *channel_ = nullptr;
};

} // namespace kb::log

#define klog kb::log::EntryBuilder(__LINE__, __FILE__, __PRETTY_FUNCTION__)