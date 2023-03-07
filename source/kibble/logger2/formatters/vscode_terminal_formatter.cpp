#include "vscode_terminal_formatter.h"
#include "../channel.h"
#include "../entry.h"
#include <array>
#include <fmt/color.h>
#include <fmt/core.h>

namespace kb::log
{

constexpr std::array<std::string_view, 6> k_icons = {
    "\033[1;48;2;50;10;10m \u2021 \033[1;49m", // fatal
    "\033[1;48;2;50;10;10m \u2020 \033[1;49m", // error
    "\033[1;48;2;50;40;10m \u203C \033[1;49m", // warn
    "\033[1;48;2;20;10;50m \u2055 \033[1;49m", // info
    " \u25B6 ",                                // debug
    " \u25B7 ",                                // verbose
};

constexpr std::array<fmt::color, 6> k_text_color = {
    fmt::color::red,        // fatal
    fmt::color::orange_red, // error
    fmt::color::orange,     // warn
    fmt::color::white,      // info
    fmt::color::white,      // debug
    fmt::color::white       // verbose
};

std::string VSCodeTerminalFormatter::format_string(const LogEntry &, const ChannelPresentation &)
{
    return "";
}

void VSCodeTerminalFormatter::print(const LogEntry &e, const ChannelPresentation &chan)
{
    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();
    // clang-format off
    fmt::print("{:6.f} {} {} {}\n",
        fmt::styled(ts, fmt::fg(fmt::color::dark_green)),
        chan.tag,
        k_icons[size_t(e.severity)],
        fmt::styled(e.message, fmt::fg(k_text_color[size_t(e.severity)]))
    );
    // clang-format on

    // print context info if present
    if (e.source_file)
    {
        // clang-format off
        fmt::print("@ {}\n{}:{}\n", 
            e.source_function, 
            fmt::styled(e.source_file, fmt::emphasis::underline),
            e.source_line
        );
        // clang-format on
    }
}

} // namespace kb::log