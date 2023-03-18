#pragma once

#include "channel.h"
#include "entry.h"
#include "severity.h"
#include <fmt/core.h>
#include <cstdlib>

namespace kb::log
{

struct EntryBuilder : private LogEntry
{
public:
    EntryBuilder(const Channel &channel, int source_line, const char *source_file, const char *source_function);
    EntryBuilder(const Channel *channel, int source_line, const char *source_file, const char *source_function);

    inline EntryBuilder &raw()
    {
        raw_text = true;
        return *this;
    }

    inline EntryBuilder &uid(std::string &&uid_str)
    {
        uid_text = std::move(uid_str);
        return *this;
    }

    inline void verbose(std::string_view sv)
    {
        log(Severity::Verbose, sv);
    }

    template <typename... ArgsT>
    inline void verbose(fmt::format_string<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Verbose, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void debug(std::string_view sv)
    {
        log(Severity::Debug, sv);
    }

    template <typename... ArgsT>
    inline void debug(fmt::format_string<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Debug, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void info(std::string_view sv)
    {
        log(Severity::Info, sv);
    }

    template <typename... ArgsT>
    inline void info(fmt::format_string<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Info, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void warn(std::string_view sv)
    {
        log(Severity::Warn, sv);
    }

    template <typename... ArgsT>
    inline void warn(fmt::format_string<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Warn, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void error(std::string_view sv)
    {
        log(Severity::Error, sv);
    }

    template <typename... ArgsT>
    inline void error(fmt::format_string<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Error, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void fatal(std::string_view sv)
    {
        log(Severity::Fatal, sv);
        exit(0);
    }

    template <typename... ArgsT>
    inline void fatal(fmt::format_string<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Fatal, fmt::format(fstr, std::forward<ArgsT>(args)...));
        exit(0);
    }

private:
    void log(Severity s, std::string_view m);

private:
    const Channel *channel_ = nullptr;
};

} // namespace kb::log
