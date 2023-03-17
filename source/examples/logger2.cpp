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
    klog2(chan).verbose("Verbose");
    klog2(chan).debug("Debug");
    klog2(chan).info("Info");
    klog2(chan).warn("Warn");
    klog2(chan).error("Error");
    klog2(chan).fatal("Fatal");
}

void some_other_func(Channel &chan)
{
    klog2(chan).verbose("Verbose");
    klog2(chan).debug("Debug");
    klog2(chan).info("Info");
    klog2(chan).warn("Warn");
    klog2(chan).error("Error");
    klog2(chan).fatal("Fatal");
}

void baz(Channel &chan)
{
    klog2(chan).warn("Warning message does not trigger a stack trace.");
    klog2(chan).error("Error message triggers a stack trace.");
}

void bar(Channel &chan)
{
    baz(chan);
}

void foo(Channel &chan)
{
    bar(chan);
}

int main()
{
    // TMP
    init_logger();

    // * Job system configuration, so we can use the logger in async mode
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

    // * Create shared objects for the logger
    // This console formatter is optimized for the VSCode integrated terminal:
    // you can ctrl+click on file paths to jump to the relevant code section in the editor
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    // This sink is responsible for printing stuff to the terminal
    auto console_sink = std::make_shared<ConsoleSink>();
    // It uses the aforementioned formatter
    console_sink->set_formatter(console_formatter);
    // This sink will dump the data it receives to a text file
    auto file_sink = std::make_shared<FileSink>("test.log");

    // Set logger in async mode by providing a JobSystem instance
    Channel::set_async(js);

    // * Create and configure channels
    // Here, we choose to log messages with severity of at least Verbose level (all messages)
    // The channel short name is "gfx" and the channel tag is displayed in crimson color in sinks that can display color
    Channel chan_graphics(Severity::Verbose, "graphics", "gfx", kb::col::crimson);
    // This channel will log to the console only
    chan_graphics.attach_sink(console_sink);

    // This channel will only record messages with severity of at least Warn level (so Warn, Error and Fatal)
    Channel chan_sound(Severity::Warn, "sound", "snd", kb::col::lightorange);
    // This channel will log to the console and to a file
    chan_sound.attach_sink(console_sink);
    chan_sound.attach_sink(file_sink);

    // This channel will only record messages with severity of at least Debug level (so Debug, Info, Warn, Error and Fatal)
    Channel chan_filesystem(Severity::Debug, "filesystem", "fs ", kb::col::deeppink);
    chan_filesystem.attach_sink(console_sink);
    // All messages with severity above Error level (included) will trigger a stack trace
    chan_filesystem.attach_policy(std::make_shared<StackTracePolicy>(Severity::Error));

    // * Let's log stuff
    // We create a task on thread 2 that will spam messages every millisecond or so
    // In asynchronous mode, the logger is able to tell which thread is logging
    // These messages will mention "T2" at the beginning
    js->create_task(
          [&chan_sound]() {
              for (size_t ii = 0; ii < 8; ++ii)
              {
                  klog2(chan_sound).warn("Hello from sound thread #{}", ii);
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
              }
          },
          {kb::th::force_worker(2), "Task"})
        .schedule();

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // This shows how each severity level is displayed
    some_func(chan_graphics);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // This shows that only warning messages and above are displayed by this channel
    some_other_func(chan_sound);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // Test the stack trace
    foo(chan_filesystem);

    // Formatted text is handled by FMTlib
    klog2(chan_graphics).verbose("Hello {} {} {}", "world", 2, -5.6f);


    // * Wait for tasks to finish, and end program
    js->wait();
    delete js;
    session->write("logger2_profile.json");
    delete session;

    return 0;
}