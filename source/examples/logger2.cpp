#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/policies/stack_trace_policy.h"
#include "logger2/sinks/console_sink.h"
#include "logger2/sinks/file_sink.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job/job_system.h"
#include "time/instrumentation.h"

#include "math/color.h"
#include "math/color_table.h"
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/os.h>

#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

using namespace kb::log;

// TMP
void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("example", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("thread", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<kb::klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void some_func(Channel &chan)
{
    // clang-format off
    klog2.chan(chan)
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
    klog2.chan(chan)
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
    klog2.chan(chan).warn("Warning message does not trigger a stack trace.");
    klog2.chan(chan).error("Error message triggers a stack trace.");
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
    klog2.chan(chan).info(fmt::format(fstr, std::forward<ArgsT>(args)...));
}

int main()
{
    // TMP
    init_logger();

    // First, we create a scheme to configure the job system
    kb::th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.max_stealing_attempts = 16;

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    kb::memory::HeapArea area(kb::th::JobSystem::get_memory_requirements());

    auto *js = new kb::th::JobSystem(area, scheme);

    // Job system profiling
    auto *session = new kb::InstrumentationSession();
    js->set_instrumentation_session(session);

    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    auto file_sink = std::make_shared<FileSink>("test.log");

    Channel chan_graphics(Severity::Verbose, "graphics", "gfx", kb::col::crimson);
    chan_graphics.attach_sink(console_sink);
    chan_graphics.set_async(js);

    Channel chan_sound(Severity::Warn, "sound", "snd", kb::col::lightorange);
    chan_sound.attach_sink(console_sink);
    chan_sound.attach_sink(file_sink);
    chan_sound.set_async(js);

    Channel chan_filesystem(Severity::Debug, "filesystem", "fs ", kb::col::deeppink);
    chan_filesystem.attach_sink(console_sink);
    chan_filesystem.attach_policy(std::make_shared<StackTracePolicy>(Severity::Error));
    chan_filesystem.set_async(js);

    some_func(chan_graphics);
    some_other_func(chan_sound);

    foo(chan_filesystem);

    // formatted(chan_graphics, "Hello");
    formatted(chan_graphics, "Hello {} {} {}", "world", 2, -5.6f);

    js->wait();

    delete js;
    session->write("logger2_profile.json");
    delete session;

    return 0;
}