#pragma once

#include "channel.h"
#include "entry.h"
#include "severity.h"

namespace kb::log
{

struct EntryBuilder : private LogEntry
{
public:
    EntryBuilder(const char *source_file, const char *source_function, int source_line)
        : LogEntry{.source_location = {source_file, source_function, source_line},
                   .timestamp = kb::TimeBase::timestamp()}
    {
    }

    inline EntryBuilder &chan(Channel &channel)
    {
        channel_ = &channel;
        return *this;
    }

    EntryBuilder &verbose(const std::string &m)
    {
        severity = Severity::Verbose;
        message = std::move(m);
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

    EntryBuilder &debug(const std::string &m)
    {
        severity = Severity::Debug;
        message = std::move(m);
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

    EntryBuilder &info(const std::string &m)
    {
        severity = Severity::Info;
        message = std::move(m);
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

    EntryBuilder &warn(const std::string &m)
    {
        severity = Severity::Warn;
        message = std::move(m);
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

    EntryBuilder &error(const std::string &m)
    {
        severity = Severity::Error;
        message = std::move(m);
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

    EntryBuilder &fatal(const std::string &m)
    {
        severity = Severity::Fatal;
        message = std::move(m);
        if (channel_)
            channel_->submit(*this);
        return *this;
    }

private:
    Channel *channel_ = nullptr;
};

}

#define klog kb::log::EntryBuilder(__FILE__, __PRETTY_FUNCTION__, __LINE__)