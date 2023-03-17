#pragma once

#include "channel.h"
#include "entry.h"
#include "severity.h"
#include <fmt/core.h>

namespace kb::log
{

template <typename... ArgsT>
using format_string_t = fmt::v9::format_string<ArgsT...>;

struct EntryBuilder : private LogEntry
{
public:
    EntryBuilder(const Channel &channel, int source_line, const char *source_file, const char *source_function);
    EntryBuilder(const Channel *channel, int source_line, const char *source_file, const char *source_function);

    template <typename... ArgsT>
    inline void verbose(format_string_t<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Verbose, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    template <typename... ArgsT>
    inline void debug(format_string_t<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Debug, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    template <typename... ArgsT>
    inline void info(format_string_t<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Info, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    template <typename... ArgsT>
    inline void warn(format_string_t<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Warn, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    template <typename... ArgsT>
    inline void error(format_string_t<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Error, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    template <typename... ArgsT>
    inline void fatal(format_string_t<ArgsT...> fstr, ArgsT &&...args)
    {
        log(Severity::Fatal, fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

private:
    void log(Severity s, std::string_view m);

private:
    const Channel *channel_ = nullptr;
};

} // namespace kb::log
