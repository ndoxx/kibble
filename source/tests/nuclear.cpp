#include "argparse/argparse.h"
#include "logger/logger.h"
#include "logger/logger_sink.h"
#include "logger/logger_thread.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job.h"
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

int p0(int argc, char** argv)
{
    ap::ArgParse parser("nuclear", "0.1");
    const auto& work_stealing_disabled = parser.add_flag('S', "disable-steal", "Disable work stealing");
    const auto& foreground_work_disabled = parser.add_flag('F', "disable-fg-work", "Disable foreground work");
    const auto& nworkers = parser.add_variable<int>('j', "threads", "Number of worker threads", 0);
    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    KLOGN("nuclear") << "[JobSystem Example] parallel for" << std::endl;

    JobSystemScheme scheme;
    scheme.max_threads = size_t(nworkers());
    scheme.enable_work_stealing = !work_stealing_disabled();
    scheme.enable_foreground_work = !foreground_work_disabled();

    memory::HeapArea area(1_MB);
    JobSystem js(area, scheme);

    constexpr size_t nexp = 1000;
    constexpr size_t len = 8192 * 32;
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

    KLOG("nuclear", 1) << "Parallel" << std::endl;
    for(size_t kk = 0; kk < nexp; ++kk)
    {
        const auto& cdata = data;
        for(size_t ii = 0; ii < njobs; ++ii)
        {
            js.dispatch([&cdata, &means, ii]() {
                means[ii] = float(std::accumulate(cdata.begin() + long(ii * tasklen),
                                                  cdata.begin() + long((ii + 1) * tasklen), 0l)) /
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

    float gain_percent = 100.f * (pmean - smean) / smean;
    float factor = float(smean) / float(pmean);
    KLOG("nuclear", 1) << "Factor: " << (factor>1 ? WCC('g') : WCC('b')) << factor << WCC(0) << std::endl;
    KLOG("nuclear", 1) << "Gain:   " << (factor>1 ? WCC('g') : WCC('b')) << gain_percent << WCC(0) << '%' << std::endl;

    return 0;
}

template <typename T, typename Iter> void random_fill(Iter start, Iter end, T min, T max)
{
    static std::random_device rd;
    std::uniform_int_distribution<T> dist(min, max);
    std::generate(start, end, [&]() { return dist(rd); });
}

int p1(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    KLOGN("nuclear") << "[JobSystem Example] mock async loading" << std::endl;

    JobSystemScheme scheme;
    scheme.max_threads = 0;
    scheme.enable_work_stealing = true;
    scheme.enable_foreground_work = true;
    scheme.scheduling_algorithm = SchedulingAlgorithm::min_load;

    memory::HeapArea area(1_MB);
    JobSystem js(area, scheme);

    constexpr size_t nexp = 4;
    constexpr size_t nloads = 100;
    std::array<long, nloads> load_time;
    random_fill(load_time.begin(), load_time.end(), 10l, 100l);
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);

    KLOG("nuclear", 1) << "Assets loading time:" << std::endl;
    for(size_t ii = 0; ii < nloads; ++ii)
    {
        KLOGI << load_time[ii] << ' ';
    }
    KLOGI << std::endl;

    for(size_t kk=0; kk<nexp; ++kk)
    {
        KLOG("nuclear", 1) << "Round " << kk << std::endl;
        milliClock clk;
        for(size_t ii = 0; ii < nloads; ++ii)
        {
            hash_t label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            js.dispatch([&load_time, ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
                // KLOG("nuclear", 1) << "Asset #" << ii << " loaded" << std::endl;
            }, label);
        }
        js.update();
        js.wait();
        auto parallel_dur_ms = clk.get_elapsed_time().count();

        float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
        float factor = float(serial_dur_ms) / float(parallel_dur_ms);
        KLOGI << "Estimated serial time: " << serial_dur_ms << "ms" << std::endl;
        KLOGI << "Parallel time:         " << parallel_dur_ms << "ms" << std::endl;
        KLOGI << "Factor:                " << (factor>1 ? WCC('g') : WCC('b')) << factor << WCC(0) << std::endl;
        KLOGI << "Gain:                  " << (factor>1 ? WCC('g') : WCC('b')) << gain_percent << WCC(0) << '%' << std::endl;
    }

    return 0;
}

int main(int argc, char** argv)
{
    init_logger();

    // return p0(argc, argv);
    return p1(argc, argv);
}