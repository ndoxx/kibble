#include "argparse/argparse.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job/job_system.h"
#include "time/clock.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numeric>
#include <random>
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
    KLOGGER(set_backtrace_on_error(false));
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

template <typename T, typename Iter> void random_fill(Iter start, Iter end, T min, T max, uint64_t seed = 0)
{
    static std::default_random_engine eng(seed);
    std::uniform_int_distribution<T> dist(min, max);
    std::generate(start, end, [&]() { return dist(eng); });
}

int p1(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    KLOGN("nuclear") << "[JobSystem Example] mock async loading" << std::endl;

    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.enable_work_stealing = true;
    scheme.scheduling_algorithm = th::SchedulingAlgorithm::min_load;
    // scheme.scheduling_algorithm = th::SchedulingAlgorithm::round_robin;

    memory::HeapArea area(1_MB);
    th::JobSystem js(area, scheme);
    js.use_persistence_file("p1.jpp");

    constexpr size_t nexp = 4;
    constexpr size_t nloads = 100;
    std::array<long, nloads> load_time;
    random_fill(load_time.begin(), load_time.end(), 1l, 100l, 42);
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);

    KLOG("nuclear", 1) << "Assets loading time:" << std::endl;
    for(size_t ii = 0; ii < nloads; ++ii)
    {
        KLOGI << load_time[ii] << ' ';
    }
    KLOGI << std::endl;

    for(size_t kk = 0; kk < nexp; ++kk)
    {
        KLOG("nuclear", 1) << "Round " << kk << std::endl;
        std::vector<th::Job*> jobs;
        milliClock clk;
        for(size_t ii = 0; ii < nloads; ++ii)
        {
            th::JobMetadata meta;
            meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            auto* job = js.create_job(
                [&load_time, ii]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
                    // KLOG("nuclear", 1) << "Asset #" << ii << " loaded" << std::endl;
                },
                meta);
            js.schedule(job);
            jobs.push_back(job);
        }
        // js.update();
        js.wait();
        auto parallel_dur_ms = clk.get_elapsed_time().count();

        float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
        float factor = float(serial_dur_ms) / float(parallel_dur_ms);
        KLOGI << "Estimated serial time: " << serial_dur_ms << "ms" << std::endl;
        KLOGI << "Parallel time:         " << parallel_dur_ms << "ms" << std::endl;
        KLOGI << "Factor:                " << (factor > 1 ? WCC('g') : WCC('b')) << factor << WCC(0) << std::endl;
        KLOGI << "Gain:                  " << (factor > 1 ? WCC('g') : WCC('b')) << gain_percent << WCC(0) << '%'
              << std::endl;

        for(auto* job : jobs)
            js.release_job(job);
    }

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    init_logger();

    return p1(argc, argv);
}