#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/policies/stack_trace_policy.h"
#include "logger2/sinks/console_sink.h"
#include "logger2/sinks/file_sink.h"

#include "math/color.h"
#include "math/color_table.h"
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/os.h>

using namespace kb::log;

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

void baz(Channel &chan)
{
    klog.chan(chan).warn("Warning message does not trigger a stack trace.");
    klog.chan(chan).error("Error message triggers a stack trace.");
}

void bar(Channel &chan)
{
    baz(chan);
}

void foo(Channel &chan)
{
    bar(chan);
}

template <typename... ArgsT>
void formatted(Channel &chan, fmt::v9::format_string<ArgsT...> fstr, ArgsT &&...args)
{
    klog.chan(chan).info(fmt::format(fstr, std::forward<ArgsT>(args)...));
}

int main()
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    auto file_sink = std::make_shared<FileSink>("test.log");

    Channel chan_graphics(Severity::Verbose, "graphics", "gfx", kb::col::crimson);
    chan_graphics.attach_sink(console_sink);

    Channel chan_sound(Severity::Warn, "sound", "snd", kb::col::lightorange);
    chan_sound.attach_sink(console_sink);
    chan_sound.attach_sink(file_sink);

    Channel chan_filesystem(Severity::Debug, "filesystem", "fs ", kb::col::deeppink);
    chan_filesystem.attach_sink(console_sink);
    chan_filesystem.attach_policy(std::make_shared<StackTracePolicy>(Severity::Error));

    some_func(chan_graphics);
    some_other_func(chan_sound);

    foo(chan_filesystem);

    formatted(chan_graphics, "Hello");
    formatted(chan_graphics, "Hello {} {} {}", "world", 2, -5.6f);

    auto out = fmt::output_file("guide.txt");
    out.print("Don't {}", "Panic");

    return 0;
}