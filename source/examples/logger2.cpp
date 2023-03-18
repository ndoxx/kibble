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

using namespace kb::log;

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

    // * Create a few logging channels for the Kibble systems we're going to use
    // This is optional
    Channel chan_memory(Severity::Verbose, "memory", "mem", kb::col::aliceblue);
    chan_memory.attach_sink(console_sink);
    Channel chan_thread(Severity::Verbose, "thread", "thd", kb::col::aquamarine);
    chan_thread.attach_sink(console_sink);

    // * Job system configuration, so we can use the logger in async mode
    // Here, we pass the "memory" logging channel to the HeapArea object, so it can log allocations
    kb::memory::HeapArea area(kb::th::JobSystem::get_memory_requirements(), &chan_memory);

    // Here, we pass the "thread" logging channel to the JobSystem object, so it can log its status
    auto *js = new kb::th::JobSystem(area, {}, &chan_thread);

    // Job system profiling: this will output a json file that can be viewed in the chrome tracing utility
    auto *session = new kb::InstrumentationSession();
    js->set_instrumentation_session(session);

    // Set logger in async mode by providing a JobSystem instance
    // By default, thread #1 is used for logging, this is an optional argument of set_async()
    // When the job system is killed, it will automatically switch the logger back to synchronous mode
    Channel::set_async(js);

    // * Create and configure test channels
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

    // This channel will only record messages with severity of at least Debug level (so Debug, Info, Warn, Error and
    // Fatal)
    Channel chan_filesystem(Severity::Debug, "filesystem", "fs ", kb::col::deeppink);
    chan_filesystem.attach_sink(console_sink);
    // All messages with severity above Error level (included) will trigger a stack trace
    chan_filesystem.attach_policy(std::make_shared<StackTracePolicy>(Severity::Error));

    // * Let's log stuff
    // We create a task on thread 2 that will spam messages every millisecond or so
    // In asynchronous mode, the logger is able to tell which thread issued the log entry
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