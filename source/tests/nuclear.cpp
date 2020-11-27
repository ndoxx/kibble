#include "argparse/argparse.h"
#include "logger/logger.h"
#include "logger/logger_sink.h"
#include "logger/logger_thread.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/atomic_queue.h"
#include "thread/job.h"
#include "time/clock.h"

#include <cmath>
#include <numeric>
#include <regex>
#include <string>
#include <vector>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("thread", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_single_threaded(true));
    KLOGGER(set_backtrace_on_error(false));
    KLOGGER(spawn());
    KLOGGER(sync());
}

void show_error_and_die(ap::ArgParse& parser)
{
    for(const auto& msg : parser.get_errors())
        KLOGW("kibble") << msg << std::endl;

    KLOG("kibble", 1) << parser.usage() << std::endl;
    exit(0);
}

std::pair<float, float> stats(const std::vector<long> durations)
{
    float mu = float(std::accumulate(durations.begin(), durations.end(), 0)) / float(durations.size());
    float variance = 0.f;
    for(long d : durations)
        variance += (float(d) - mu) * (float(d) - mu);
    variance /= float(durations.size());
    return {mu, std::sqrt(variance)};
}

struct Plop
{
    int a = 0;
    int b = 42;
};

int main(int argc, char** argv)
{
    init_logger();

    ap::ArgParse parser("nuclear", "0.1");
    const auto& work_stealing_disabled = parser.add_flag('S', "disable-steal", "Disable work stealing");
    const auto& foreground_work_disabled = parser.add_flag('F', "disable-fg-work", "Disable foreground work");
    const auto& nworkers = parser.add_variable<int>('j', "threads", "Number of worker threads", 0);
    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    KLOGN("nuclear") << "Start" << std::endl;

    JobSystemScheme scheme;
    scheme.max_threads = size_t(nworkers());
    scheme.enable_work_stealing = !work_stealing_disabled();
    scheme.enable_foreground_work = !foreground_work_disabled();

    memory::HeapArea area(1_MB);
    JobSystem js(area, scheme);

    constexpr size_t nexp = 1000;
    constexpr size_t len = 8192*32;
    constexpr size_t njobs = 256;
    constexpr size_t tasklen = len / njobs;

    std::vector<long> durations(nexp);
    std::vector<float> results(nexp);
    std::vector<float> data(len);
    std::array<float, njobs> means;
    std::iota(data.begin(), data.end(), 1.f);
    std::fill(means.begin(), means.end(), 0.f);

    KLOG("nuclear", 1) << "Serial" << std::endl;
    for(size_t kk = 0; kk < nexp; ++kk)
    {
        microClock clk;
        float mean = float(std::accumulate(data.begin(), data.end(), 0l)) / float(len);
        durations[kk] = clk.get_elapsed_time().count();
        results[kk] = mean;
    }

    auto [smean, sstd] = stats(durations);
    KLOGI << "Mean active time:   " << smean << "us" << std::endl;
    KLOGI << "Standard deviation: " << sstd << "us" << std::endl;

    for(size_t kk = 0; kk < nexp; ++kk)
    {
        const auto& cdata = data;
        for(size_t ii = 0; ii < njobs; ++ii)
        {
            js.schedule([&cdata, &means, ii]() {
                means[ii] =
                    float(std::accumulate(cdata.begin() + long(ii * tasklen), cdata.begin() + long((ii + 1) * tasklen), 0l)) /
                    float(tasklen);
            });
        }

        microClock clk;
        js.update();
        js.wait();

        float mean = float(std::accumulate(means.begin(), means.end(), 0l)) / float(njobs);
        std::fill(means.begin(), means.end(), 0.f);
        durations[kk] = clk.get_elapsed_time().count();
        results[kk] = mean;
    }

    auto [pmean, pstd] = stats(durations);
    KLOGI << "Mean active time:   " << pmean << "us" << std::endl;
    KLOGI << "Standard deviation: " << pstd << "us" << std::endl;

    float gain_percent = 100.f * (smean - pmean) / smean;
    KLOG("nuclear", 1) << "Gain: " << gain_percent << '%' << std::endl;

    return 0;
}