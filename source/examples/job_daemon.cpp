#include "argparse/argparse.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
#include "memory/heap_area.h"
#include "thread/job/config.h"
#include "thread/job/daemon.h"
#include "thread/job/job_system.h"
#include "time/clock.h"
#include "time/instrumentation.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace kb;
using namespace kb::th;
using namespace kb::log;

void show_error_and_die(ap::ArgParse &parser, const Channel &chan)
{
    for (const auto &msg : parser.get_errors())
        klog2(chan).warn(msg);

    klog2(chan).raw().info(parser.usage());
    exit(0);
}

int main(int argc, char **argv)
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan_kibble(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan_kibble.attach_sink(console_sink);
    Channel chan_thread(Severity::Verbose, "thread", "thd", kb::col::crimson);
    chan_thread.attach_sink(console_sink);

    ap::ArgParse parser("daemon_example", "0.1");
    parser.set_log_output([&chan_kibble](const std::string &str) { klog2(chan_kibble).uid("ArgParse").info(str); });
    const auto &nf = parser.add_variable<int>('f', "frames", "Number of frames", 200);
    const auto &nj = parser.add_variable<int>('j', "jobs", "Number of jobs", 50);

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan_kibble);

    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.max_stealing_attempts = 16;

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    memory::HeapArea area(th::JobSystem::get_memory_requirements());

    auto *js = new th::JobSystem(area, scheme, &chan_thread);
    auto *ds = new th::DaemonScheduler(*js);

    Channel::set_async(js);

    // Job system profiling
    auto *session = new InstrumentationSession();
    js->set_instrumentation_session(session);

    // Data for the daemons
    struct Message
    {
        std::string message;
        float interval_ms;
        int64_t ttl = 0;
    };

    // Daemons with a default ttl of 0 will run till they are killed.
    // The second one has a ttl of 4 and will only execute 4 times.
    // The cooldown_ms property could also be initialized with a positive
    // value to delay execution.
    std::vector<Message> msgs = {{"hello", 100.f}, {"salut", 200.f, 4}, {"sunt eu", 500.f}, {"un haiduc", 1000.f}};
    std::vector<DaemonHandle> hnds;

    // Launch daemons
    for (const auto &msg : msgs)
    {
        th::JobMetadata meta(th::WORKER_AFFINITY_ASYNC, msg.message);

        th::SchedulingData sd;
        // Set the interval in ms at which the tasks will be rescheduled
        sd.interval_ms = msg.interval_ms;
        // Set the number of repetitions
        sd.ttl = msg.ttl;

        // Daemons will just log a message and wait a bit
        auto hnd = ds->create(
            [msg, &chan_kibble]() {
                klog2(chan_kibble).uid("Daemon").info(msg.message);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            },
            sd, meta);

        // Save handles, in case we need to kill daemons while they're running
        hnds.push_back(hnd);
    }

    // Simulate game loop
    size_t nframes = size_t(nf());
    for (size_t ii = 0; ii < nframes; ++ii)
    {
        microClock clk;
        // Call this function each frame so daemons can be rescheduled
        ds->update();

        // Create a few independent tasks each frame
        for (size_t jj = 0; jj < size_t(nj()); ++jj)
        {
            auto tsk = js->create_task([]() { std::this_thread::sleep_for(std::chrono::microseconds(500)); },
                                       th::JobMetadata(th::WORKER_AFFINITY_ANY, "job"));
            tsk.schedule();
        }

        // Kill the first daemon manually after some time
        if (ii == nframes / 2)
            ds->kill(hnds[0]);

        // Wait on all jobs to finish
        js->wait();
    }

    delete ds;
    delete js;

    session->write("example_daemon.json");
    delete session;

    return 0;
}