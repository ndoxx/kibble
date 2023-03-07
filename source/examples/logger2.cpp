#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/policies/stack_trace_policy.h"
#include "logger2/sinks/console_sink.h"

#include "math/color.h"
#include "math/color_table.h"
#include <fmt/color.h>
#include <fmt/core.h>

using namespace kb::log;

inline auto to_rgb(kb::math::argb32_t color)
{
    return fmt::rgb{uint8_t(color.r()), uint8_t(color.g()), uint8_t(color.b())};
}

std::string create_channel_tag(const std::string &short_name, kb::math::argb32_t color)
{
    return fmt::format(
        "{}", fmt::styled(short_name, fmt::bg(to_rgb(color)) | fmt::fg(fmt::color::white) | fmt::emphasis::bold));
}

void some_func(Channel &chan)
{
    // clang-format off
    klog.chan(chan)
        .verbose("Verbose")
        .debug("Debug")
        .info("Info")
        .warn("Warn")
        .error("Error")
        .fatal("Fatal");
    // clang-format on
}

void some_other_func(Channel &chan)
{
    // clang-format off
    klog.chan(chan)
        .verbose("Verbose")
        .debug("Debug")
        .info("Info")
        .warn("Warn")
        .error("Error")
        .fatal("Fatal");
    // clang-format on
}

void foo(Channel &chan)
{
    klog.chan(chan).warn("Warning message does not trigger a stack trace.");
    klog.chan(chan).error("Error message triggers a stack trace.");
}

void bar(Channel &chan)
{
    foo(chan);
}

void baz(Channel &chan)
{
    bar(chan);
}

int main()
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);

    Channel chan_graphics(Severity::Verbose, {"graphics", create_channel_tag("gfx", kb::col::crimson)});
    chan_graphics.attach_sink(console_sink);

    Channel chan_sound(Severity::Warn, {"sound", create_channel_tag("snd", kb::col::lightorange)});
    chan_sound.attach_sink(console_sink);

    Channel chan_filesystem(Severity::Debug, {"filesystem", create_channel_tag("fs ", kb::col::deeppink)});
    chan_filesystem.attach_sink(console_sink);
    chan_filesystem.attach_policy(std::make_shared<StackTracePolicy>(Severity::Error));

    some_func(chan_graphics);
    some_other_func(chan_sound);

    baz(chan_filesystem);

    return 0;
}