#include "job_example.h"

#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
#include "time/instrumentation.h"

#include <filesystem>
#include <numeric>

using namespace kb;
using namespace kb::log;

std::pair<float, float> stats(const std::vector<long> durations)
{
    float mu = float(std::accumulate(durations.begin(), durations.end(), 0)) / float(durations.size());
    float variance = 0.f;
    for (long d : durations)
        variance += (float(d) - mu) * (float(d) - mu);
    variance /= float(durations.size());
    return {mu, std::sqrt(variance)};
}

void show_statistics(kb::milliClock& clk, long serial_dur_ms, const kb::log::Channel& chan)
{
    auto parallel_dur_ms = clk.get_elapsed_time().count();
    float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
    float factor = float(serial_dur_ms) / float(parallel_dur_ms);
    klog(chan).verbose("Estimated serial time: {}ms", serial_dur_ms);
    klog(chan).verbose("Parallel time:         {}ms", parallel_dur_ms);
    klog(chan).verbose("Factor:                {}", factor);
    klog(chan).verbose("Gain:                  {}%", gain_percent);
}

void JobExample::show_error_and_die(kb::ap::ArgParse& parser, const kb::log::Channel& chan)
{
    for (const auto& msg : parser.get_errors())
        klog(chan).warn(msg);

    klog(chan).raw().info(parser.usage());
    exit(0);
}

int JobExample::run(int argc, char** argv)
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan_kibble(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan_kibble.attach_sink(console_sink);
    Channel chan_thread(Severity::Verbose, "thread", "thd", kb::col::crimson);
    chan_thread.attach_sink(console_sink);
    Channel chan_memory(Severity::Verbose, "memory", "mem", kb::col::ndxorange);
    chan_memory.attach_sink(console_sink);

    ap::ArgParse parser("job_system_example", "0.1");
    parser.set_log_output([&chan_kibble](const std::string& str) { klog(chan_kibble).uid("ArgParse").info(str); });
    const auto& ne = parser.add_variable<int>('e', "experiments", "Number of experiments to perform", 4);
    const auto& nj = parser.add_variable<int>('j', "jobs", "Number of jobs", 100);

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan_kibble);

    size_t nexp = std::min(size_t(ne()), 100ul);
    size_t njob = std::min(size_t(nj()), 500ul);

    // First, we create a scheme to configure the job system
    using namespace std::literals::string_literals;

    th::JobSystem::Config scheme;
    scheme.max_workers = 0;
    scheme.max_stealing_attempts = 16;
    scheme.max_barriers = 8;

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    memory::HeapArea area(th::JobSystem::get_memory_requirements(scheme), &chan_memory);

    auto* js = new th::JobSystem(area, scheme, &chan_thread);
    Channel::set_async(js);

    // Job system profiling
#ifdef KB_JOB_SYSTEM_PROFILING
    auto* session = new InstrumentationSession();
    js->set_instrumentation_session(session);
#endif

    int ret = impl(nexp, njob, *js, chan_kibble);

    delete js;

#ifdef KB_JOB_SYSTEM_PROFILING
    std::filesystem::path filepath = fmt::format("job_example_{}.json", fs::path(argv[0]).stem().string());
    klog(chan_kibble).info("Writing profiling data to {}", filepath.string());
    session->write(filepath);
    delete session;
#endif

    return ret;
}
