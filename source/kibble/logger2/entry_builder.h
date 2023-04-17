#pragma once

#include "channel.h"
#include "entry.h"
#include "severity.h"
#include <fmt/core.h>

namespace kb::log
{

/**
 * @brief Helper class to generate a log entry
 *
 */
struct EntryBuilder : private LogEntry
{
public:
    EntryBuilder(const Channel& channel, int source_line, const char* source_file, const char* source_function);
    EntryBuilder(const Channel* channel, int source_line, const char* source_file, const char* source_function);

    /**
     * @brief Set this log entry as raw text.
     *
     * Formatters shall skip formatting contextual information and only output the raw message
     *
     * @return EntryBuilder&
     */
    inline EntryBuilder& raw()
    {
        raw_text = true;
        return *this;
    }

    inline EntryBuilder& level(Severity s)
    {
        severity = s;
        return *this;
    }

    /**
     * @brief Attach a UID to this log entry.
     *
     * UIDs help better understand what subsystem issued this particular logging call.
     * They can also be whitelisted / blacklisted by policies.
     *
     * @param uid_str
     * @return EntryBuilder&
     */
    inline EntryBuilder& uid(std::string&& uid_str)
    {
        uid_text = std::move(uid_str);
        return *this;
    }

    inline void msg(std::string_view sv)
    {
        log(sv);
    }

    template <typename... ArgsT>
    inline void msg(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void verbose(std::string_view sv)
    {
        level(Severity::Verbose).log(sv);
    }

    template <typename... ArgsT>
    inline void verbose(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Verbose).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void debug(std::string_view sv)
    {
        level(Severity::Debug).log(sv);
    }

    template <typename... ArgsT>
    inline void debug(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Debug).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void info(std::string_view sv)
    {
        level(Severity::Info).log(sv);
    }

    template <typename... ArgsT>
    inline void info(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Info).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void warn(std::string_view sv)
    {
        level(Severity::Warn).log(sv);
    }

    template <typename... ArgsT>
    inline void warn(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Warn).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void error(std::string_view sv)
    {
        level(Severity::Error).log(sv);
    }

    template <typename... ArgsT>
    inline void error(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Error).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

    inline void fatal(std::string_view sv)
    {
        level(Severity::Fatal).log(sv);
    }

    template <typename... ArgsT>
    inline void fatal(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Fatal).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }

private:
    void log(std::string_view m);
    void log(std::string&& m);

private:
    const Channel* channel_ = nullptr;
};

} // namespace kb::log
