#ifndef K_DEBUG
#define K_DEBUG
#endif
#include "argparse/argparse.h"
#include "logger2/formatters/powerline_terminal_formatter.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/policies/stack_trace_policy.h"
#include "logger2/policies/uid_filter.h"
#include "logger2/sinks/console_sink.h"
#include "logger2/sinks/file_sink.h"
#include "memory/heap_area.h"
#include "memory/util/arithmetic.h"
#include "thread/job/job_system.h"
#include "time/instrumentation.h"

#include "math/color.h"
#include "math/color_table.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/os.h"
#include <iostream>

using namespace kb::log;

void show_error_and_die(kb::ap::ArgParse& parser)
{
    for (const auto& msg : parser.get_errors())
        std::cerr << msg << std::endl;

    std::cout << parser.usage() << std::endl;
    exit(0);
}

void some_func(Channel& chan)
{
    klog(chan).verbose("Verbose");
    klog(chan).debug("Debug");
    klog(chan).info("Info");
    klog(chan).warn("Warn");
    klog(chan).error("Error");
    klog(chan).fatal("Fatal");
}

void some_other_func(Channel& chan)
{
    klog(chan).verbose("Verbose");
    klog(chan).debug("Debug");
    klog(chan).info("Info");
    klog(chan).warn("Warn");
    klog(chan).error("Error");
    klog(chan).fatal("Fatal");
}

void baz(Channel& chan)
{
    klog(chan).warn("Warning message does not trigger a stack trace.");
    klog(chan).error("Error message triggers a stack trace.");
}

void bar(Channel& chan)
{
    baz(chan);
}

void foo(Channel& chan)
{
    bar(chan);
}

int main(int argc, char** argv)
{
    kb::ap::ArgParse parser("logger2_example", "0.1");
    const auto& use_powerline = parser.add_variable<bool>(
        'p', "powerline", "Use a powerline-styled terminal formatter (needs a powerline-patched font)", false);

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser);

#ifdef K_DEBUG
    std::cout << "DEBUG BUILD" << std::endl;
#else
    std::cout << "RELEASE BUILD" << std::endl;
#endif

    // * Create shared objects for the logger
    std::shared_ptr<Formatter> console_formatter;

    // These console formatters are optimized for the VSCode integrated terminal:
    // you can ctrl+click on file paths to jump to the relevant code section in the editor
    // VSCodeTerminalFormatter is a simple portable formatter.
    // PowerlineTerminalFormatter is a powerline-styled terminal formatter, much more readable,
    // but you'll need to install a powerline-patched font (https://github.com/powerline/fonts)
    // for it to work correctly.

    if (use_powerline())
        console_formatter = std::make_shared<PowerlineTerminalFormatter>();
    else
        console_formatter = std::make_shared<VSCodeTerminalFormatter>();

    // This sink is responsible for printing stuff to the terminal
    auto console_sink = std::make_shared<ConsoleSink>();
    // It uses the aforementioned formatter
    console_sink->set_formatter(console_formatter);

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
    auto* js = new kb::th::JobSystem(area, {}, &chan_thread);

    // Job system profiling: this will output a json file that can be viewed in the chrome tracing utility
    auto* session = new kb::InstrumentationSession();
    js->set_instrumentation_session(session);

    // Set logger in async mode by providing a JobSystem instance
    // By default, thread #1 is used for logging, this is an optional argument of set_async()
    // When the job system is killed, it will automatically switch the logger back to synchronous mode
    Channel::set_async(js);

    // By default, a fatal error will terminate thread execution and shutdown the program
    // We don't need this behavior here so we disable it
    Channel::exit_on_fatal_error(false);

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
    // This sink will dump the data it receives to a text file
    chan_sound.attach_sink(std::make_shared<FileSink>("test.log"));

    // This channel will only record messages with severity of at least Debug level (so Debug, Info, Warn, Error and
    // Fatal)
    Channel chan_filesystem(Severity::Debug, "filesystem", "fs ", kb::col::deeppink);
    chan_filesystem.attach_sink(console_sink);
    // All messages with severity above Error level (included) will trigger a stack trace
    chan_filesystem.attach_policy(std::make_shared<StackTracePolicy>(Severity::Error));

    // * Let's log stuff
    // Formatted text is handled by FMTlib
    klog(chan_graphics).verbose("Hello {} {} {}", "world", 2, -5.6f);
    klog(chan_graphics).verbose("I'm {} da ba dee da ba daa", fmt::styled("blue", fmt::fg(fmt::color::blue)));

    // To skip the log entry header and display raw text, chain the call after raw():
    klog(chan_graphics).raw().info("Raw text");

    // Channels can be shared by multiple subsystems, using UIDs can help better distinguish between these
    klog(chan_graphics).uid("Texture").info("Texture related stuff");
    klog(chan_graphics).uid("Backend").info("Renderer backend related stuff");
    klog(chan_graphics).uid("Mesh").info("Mesh related stuff");

    // Also, it is possible to devise a policy to filter through such UIDs:
    // Only messages with a UID set to "ResourcePack" or "CatFile" or no UID at all will be logged
    // There's also a blacklist policy available
    auto whitelist = std::make_shared<UIDWhitelist>(std::set{"ResourcePack"_h});
    whitelist->add("CatFile"_h);
    chan_filesystem.attach_policy(whitelist);
    klog(chan_filesystem).info("General filesystem info are logged");
    klog(chan_filesystem).uid("ResourcePack").info("ResourcePack info are logged");
    klog(chan_filesystem).uid("CatFile").info("CatFile info are logged");
    klog(chan_filesystem).uid("DofFile").info("DofFile info are NOT logged");

    // printf-debugging, here we come
    kbang(chan_graphics);

    // A logging channel can be used on another thread
    // We create a task on thread 2 that will spam messages every millisecond or so
    // In asynchronous mode, the logger is able to tell which thread issued the log entry
    // These messages will mention "T2" at the beginning
    auto&& [task, future] = js->create_task({kb::th::force_worker(2), "Task"}, [&chan_sound]() {
        for (size_t ii = 0; ii < 8; ++ii)
        {
            klog(chan_sound).warn("Hello from sound thread #{}", ii);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    task.schedule();

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // This shows how each severity level is displayed
    some_func(chan_graphics);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // This shows that only warning messages and above are displayed by this channel
    some_other_func(chan_sound);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // Test the stack trace
    foo(chan_filesystem);

    // * Wait for tasks to finish, and end program
    js->wait();
    delete js;
    session->write("logger2_profile.json");
    delete session;

    return 0;
}