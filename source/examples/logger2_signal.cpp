#include "argparse/argparse.h"
#include "logger2/formatters/powerline_terminal_formatter.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/policies/stack_trace_policy.h"
#include "logger2/sinks/console_sink.h"
#include "logger2/sinks/file_sink.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job/job_system.h"
#include "time/instrumentation.h"

#include <csignal>
#include <iostream>

using namespace kb::log;

void show_error_and_die(kb::ap::ArgParse &parser)
{
    for (const auto &msg : parser.get_errors())
        std::cerr << msg << std::endl;

    std::cout << parser.usage() << std::endl;
    exit(0);
}

int main(int argc, char **argv)
{
    /*
        This example showcases an experimental feature of the logger.
        It is possible to enable POSIX signal interception, and force the
        job system into panic mode when a signal is intercepted.
        In panic mode, the workers are shut down, and the main thread
        executes essential tasks in their private queues before exiting.
        The logger marks all logging tasks as essential, and is the only
        object able to do so.
        Then, all logging tasks that have been submitted before the signal
        was raised are guaranteed to be completed before the program exits.

        This program will make TSAN go mad about non-signal-safe functions
        being called in the signal handler. That's because the sinks will
        end up making string allocations (among other things) that aren't
        signal-safe.
        These warnings can be suppressed by creating a logger2.suppressions file
        with "signal:*" for content, and call the program with
        > TSAN_OPTIONS="suppressions=logger2.suppressions" path/to/bin/ex/logger2_signal

        This system works fine with std::raise(), but is utterly UB in any
        other case. Time will tell if this is a keeper...
    */

    kb::ap::ArgParse parser("logger2_example", "0.1");
    const auto &use_powerline = parser.add_variable<bool>(
        'p', "powerline", "Use a powerline-styled terminal formatter (needs a powerline-patched font)", false);

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser);

    // * Create shared objects for the logger
    std::shared_ptr<Formatter> console_formatter;

    if (use_powerline())
        console_formatter = std::make_shared<PowerlineTerminalFormatter>();
    else
        console_formatter = std::make_shared<VSCodeTerminalFormatter>();

    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);

    kb::memory::HeapArea area(kb::th::JobSystem::get_memory_requirements());

    Channel chan_thread(Severity::Verbose, "thread", "thd", kb::col::aquamarine);
    chan_thread.attach_sink(console_sink);
    auto *js = new kb::th::JobSystem(area, {}, &chan_thread);

    // Enable signal interception feature
    Channel::intercept_signals();
    // This call will register signal handlers to force the JobSystem into
    // panic mode, when a signal is intercepted
    Channel::set_async(js);

    Channel chan(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan.attach_sink(console_sink);
    Channel chan_secondary(Severity::Verbose, "secondary", "sec", kb::col::blue);
    chan_secondary.attach_sink(console_sink);

    // Spawn a few non-essential tasks, that will produce logs as a side effect
    // Only a few of these should show up
    for (size_t ii = 0; ii < 100; ++ii)
    {
        auto &&[task, future] = js->create_task({kb::th::force_worker(2), "Task"}, [ii, &chan_secondary]() {
            klog(chan_secondary).info("Unimportant task #{}", ii);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        task.schedule();
    }

    // Log a few messages
    // All of these will show up
    for (size_t ii = 0; ii < 100; ++ii)
    {
        klog(chan).info("Message #{}", ii);
    }

    // Raise a segmentation violation signal
    // Signal handler invocation is guaranteed to happen in the same thread
    // that called std::raise(). If the signal was not due to a call to std::raise(),
    // the signal handler can be called from any thread.
    // In an attempt to simulate this, I call std::raise() from another thread.
    auto &&[task, future] = js->create_task({kb::th::force_worker(3), "BadTask"}, []() { std::raise(SIGSEGV); });
    task.schedule();

    // We shouldn't reach this line
    js->wait();
    delete js;

    return 0;
}