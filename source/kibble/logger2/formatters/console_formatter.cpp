#include "console_formatter.h"
#include "../channel.h"
#include "../entry.h"
#include <array>
#include <fmt/core.h>

namespace kb::log
{

static constexpr std::string_view k_timestamp_style{"\033[1;38;2;0;130;10m"};
static constexpr std::string_view k_code_file_style{"\033[1;38;2;255;255;255m"};

constexpr std::array<std::string_view, 6> k_icons = {
    "\033[1;48;2;50;10;10m \u2021 \033[1;49m", // fatal
    "\033[1;48;2;50;10;10m \u2020 \033[1;49m", // error
    "\033[1;48;2;50;40;10m \u203C \033[1;49m", // warn
    "\033[1;48;2;20;10;50m \u2055 \033[1;49m", // info
    "\033[1;48;2;20;10;50m \u25B6 \033[1;49m", // debug
    "\033[1;48;2;20;10;50m \u25B7 \033[1;49m", // verbose
};

std::string ConsoleFormatter::format_string(const LogEntry &, const ChannelPresentation &)
{
    return "";
}

void ConsoleFormatter::print(const LogEntry &e, const ChannelPresentation &chan)
{
    // print file and line if severity high enough
    if (e.severity != Severity::Verbose && e.severity != Severity::Info)
    {
        // clang-format off
        fmt::print("[{}]{}@{}:{}\n", 
            e.source_function, 
            k_code_file_style, 
            e.source_file, 
            e.source_line
        );
        // clang-format on
    }

    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();
    // clang-format off
    fmt::print("{}[{:6.f}]\033[0m[{}] {} {}\n",
        k_timestamp_style,
        ts,
        chan.tag,
        k_icons[size_t(e.severity)],
        e.message
    );
    // clang-format on
}

} // namespace kb::log