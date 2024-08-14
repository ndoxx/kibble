#include "kibble/logger/formatters/powerline_terminal_formatter.h"
#include "kibble/logger/channel.h"
#include "kibble/logger/entry.h"

#include "fmt/color.h"
#include "fmt/format.h"
#include <array>

namespace kb::log
{

constexpr std::array<fmt::color, 6> k_severity_color = {
    fmt::color::red,        // fatal
    fmt::color::orange_red, // error
    fmt::color::orange,     // warn
    fmt::color::light_blue, // info
    fmt::color::white,      // debug
    fmt::color::light_gray  // verbose
};

inline auto to_rgb(kb::math::argb32_t color)
{
    return fmt::rgb{uint8_t(color.r()), uint8_t(color.g()), uint8_t(color.b())};
}

void PowerlineTerminalFormatter::print(const LogEntry& e, const ChannelPresentation& p)
{
    if (e.raw_text)
    {
        return fmt::print("{}\n", e.message);
    }

    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();
    auto sev_color = k_severity_color[size_t(e.severity)];
    auto tag_color = to_rgb(p.color);

    if (e.thread_id != 0xffffffff)
    {
        fmt::print("{}",
                   fmt::styled(fmt::format(fmt::runtime("T{}\u250a{:6.6f}"), e.thread_id, ts), fmt::bg(sev_color)));
    }
    else
    {
        fmt::print("{}", fmt::styled(fmt::format(fmt::runtime("{:6.6f}"), ts), fmt::bg(sev_color)));
    }

    if (e.uid_text.size() == 0)
    {
        fmt::print("{}{}{} {}\n", fmt::styled("\ue0b0", fmt::fg(sev_color) | fmt::bg(tag_color)),
                   fmt::styled(p.tag, fmt::bg(tag_color) | fmt::emphasis::bold),
                   fmt::styled("\ue0b0", fmt::fg(tag_color)), e.message);
    }
    else
    {
        fmt::print("{}{}{}{}{} {}\n", fmt::styled("\ue0b0", fmt::fg(sev_color) | fmt::bg(tag_color)),
                   fmt::styled(p.tag, fmt::bg(tag_color) | fmt::emphasis::bold),
                   fmt::styled("\ue0b0", fmt::fg(tag_color) | fmt::bg(fmt::color::white)),
                   fmt::styled(e.uid_text, fmt::bg(fmt::color::white) | fmt::emphasis::italic),
                   fmt::styled("\ue0b0", fmt::fg(fmt::color::white)), e.message);
    }

    // print context info if needed
    if (e.severity <= Severity::Warn)
    {
        // clang-format off
        fmt::print("   \u2ba1 {}\n   \u2ba1 {}:{}\n", 
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