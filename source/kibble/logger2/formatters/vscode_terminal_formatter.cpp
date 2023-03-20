#include "vscode_terminal_formatter.h"
#include "../channel.h"
#include "../entry.h"
#include <array>
#include <fmt/color.h>
#include <fmt/core.h>

namespace kb::log
{

constexpr std::array<std::string_view, 6> k_icons = {
    "\u2021", // fatal
    "\u2020", // error
    "\u203C", // warn
    "\u2055", // info
    "\u25B6", // debug
    "\u25B7", // verbose
};

constexpr std::array<fmt::color, 6> k_text_color = {
    fmt::color::red,        // fatal
    fmt::color::orange_red, // error
    fmt::color::orange,     // warn
    fmt::color::white,      // info
    fmt::color::white,      // debug
    fmt::color::white       // verbose
};

inline auto to_rgb(math::argb32_t color)
{
    return fmt::rgb{uint8_t(color.r()), uint8_t(color.g()), uint8_t(color.b())};
}

inline std::string format_uid(const std::string &input)
{
    if (input.size() == 0)
        return "";
    else
        return fmt::format("[{}] ", fmt::styled(input, fmt::emphasis::italic));
}

void VSCodeTerminalFormatter::print(const LogEntry &e, const ChannelPresentation &chan)
{
    if (e.raw_text)
        return fmt::print("{}\n", e.message);

    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();

    if (e.thread_id != 0xffffffff)
    {
        fmt::print("T{}:", e.thread_id);
    }

    // clang-format off
    fmt::print("{:6.f} {} {} {}{}\n",
        fmt::styled(ts, fmt::fg(fmt::color::dark_green)),
        fmt::styled(chan.tag, fmt::bg(to_rgb(chan.color)) | fmt::fg(fmt::color::white) | fmt::emphasis::bold),
        fmt::styled(k_icons[size_t(e.severity)], fmt::fg(k_text_color[size_t(e.severity)]) | fmt::emphasis::bold),
        format_uid(e.uid_text),
        fmt::styled(e.message, fmt::fg(k_text_color[size_t(e.severity)]))
    );
    // clang-format on

    // print context info if needed
    if (uint8_t(e.severity) <= 2)
    {
        // clang-format off
        fmt::print("@ {}\n{}:{}\n", 
            e.source_location.function_name, 
            fmt::styled(e.source_location.file_name, fmt::emphasis::underline),
            e.source_location.line
        );
        // clang-format on
    }

    // print stack trace
    if (e.stack_trace.has_value())
    {
        fmt::print("{}", e.stack_trace->format());
    }
}

} // namespace kb::log