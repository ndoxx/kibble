#pragma once

#include "kibble/logger/channel.h"
#include "kibble/logger/entry.h"
#include "kibble/logger/severity.h"

#include "fmt/core.h"
#include "fmt/format.h"

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

    // Unformatted
    inline void msg(std::string_view sv)
    {
        log(sv);
    }
    inline void verbose([[maybe_unused]] std::string_view sv)
    {
#ifdef K_DEBUG
        level(Severity::Verbose).log(sv);
#endif
    }
    inline void debug([[maybe_unused]] std::string_view sv)
    {
#ifdef K_DEBUG
        level(Severity::Debug).log(sv);
#endif
    }
    inline void info(std::string_view sv)
    {
        level(Severity::Info).log(sv);
    }
    inline void warn(std::string_view sv)
    {
        level(Severity::Warn).log(sv);
    }
    inline void error(std::string_view sv)
    {
        level(Severity::Error).log(sv);
    }
    inline void fatal(std::string_view sv)
    {
        level(Severity::Fatal).log(sv);
    }

    // Compile-time
    template <typename... ArgsT>
    inline void msg(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }
    template <typename... ArgsT>
    inline void verbose([[maybe_unused]] fmt::format_string<ArgsT...> fstr, [[maybe_unused]] ArgsT&&... args)
    {
#ifdef K_DEBUG
        level(Severity::Verbose).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
#endif
    }
    template <typename... ArgsT>
    inline void debug([[maybe_unused]] fmt::format_string<ArgsT...> fstr, [[maybe_unused]] ArgsT&&... args)
    {
#ifdef K_DEBUG
        level(Severity::Debug).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
#endif
    }
    template <typename... ArgsT>
    inline void info(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Info).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }
    template <typename... ArgsT>
    inline void warn(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Warn).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
    }
    template <typename... ArgsT>
    inline void error(fmt::format_string<ArgsT...> fstr, ArgsT&&... args)
    {
        level(Severity::Error).log(fmt::format(fstr, std::forward<ArgsT>(args)...));
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
