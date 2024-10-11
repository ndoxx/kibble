#include "kibble/logger/formatters/monochrome_terminal_formatter.h"
#include "kibble/logger/channel.h"
#include "kibble/logger/entry.h"

#include "fmt/format.h"

namespace kb::log
{

constexpr std::array<std::string_view, 6> k_icons = {
    "F", // fatal
    "E", // error
    "W", // warn
    "I", // info
    "D", // debug
    "V", // verbose
};

inline std::string format_uid(const std::string& input)
{
    if (input.size() == 0)
    {
        return "";
    }
    else
    {
        return fmt::format("{}", input);
    }
}

void MonochromeTerminalFormatter::print(const LogEntry& e, const ChannelPresentation& chan)
{
    if (e.raw_text)
    {
        fmt::print("{}\n", e.message);
        std::fflush(stdout);
        return;
    }

    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();

    fmt::print("[{}] ", k_icons[size_t(e.severity)]);

    if (e.thread_id != 0xffffffff)
    {
        fmt::print("T{}:", e.thread_id);
    }

    // clang-format off
    fmt::print(fmt::runtime("{:6.6f}>{}>{} {}\n"), ts, chan.tag,
        e.uid_text.empty() ? "" : fmt::format("{}> ",e.uid_text),
        e.message
    );
    // clang-format on

    // print context info if needed
    if (uint8_t(e.severity) <= 2)
    {
        // clang-format off
        fmt::print("@ {}\n{}:{}\n", 
            e.source_location.function_name, 
            e.source_location.file_name,
            e.source_location.line
        );
        // clang-format on
    }

    // print stack trace
    if (e.stack_trace.has_value())
    {
        fmt::print("{}", e.stack_trace->format());
    }

    std::fflush(stdout);
}

} // namespace kb::log