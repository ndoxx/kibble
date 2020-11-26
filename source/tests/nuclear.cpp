#include "logger/logger.h"
#include "logger/logger_sink.h"
#include "logger/logger_thread.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/atomic_queue.h"
#include "thread/job.h"
#include "time/clock.h"

#include <numeric>
#include <regex>
#include <string>
#include <vector>
#include <cmath>

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

std::pair<float,float> stats(const std::vector<long> durations)
{
    float mu = float(std::accumulate(durations.begin(), durations.end(), 0)) / float(durations.size());
    float variance = 0.f;
    for(long d: durations)
        variance += (float(d)-mu)*(float(d)-mu);
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
    (void)argc;
    (void)argv;

    init_logger();

    KLOGN("nuclear") << "Start" << std::endl;

    memory::HeapArea area(512_kB);
    JobSystem js(area);

    constexpr size_t nexp = 1000;
    constexpr size_t len = 8192;
    constexpr size_t njobs = 128;

    std::vector<long> durations(nexp);
    std::vector<float> results(nexp);
    std::vector<float> data(njobs * len);
    std::iota(data.begin(), data.end(), 0.f);
    std::array<float, njobs> means;

    KLOG("nuclear", 1) << "Serial" << std::endl;
    for(size_t kk = 0; kk < nexp; ++kk)
    {
        std::fill(means.begin(), means.end(), 0.f);
        microClock clk;
        float mean = 0.f;
        for(float xx : data)
            mean += xx;
        mean /= float(data.size());
        durations[kk] = clk.get_elapsed_time().count();

        results[kk] = mean;
    }

    auto [smean, sstd] = stats(durations);
    KLOGI << "Mean active time:   " << smean << "us" << std::endl;
    KLOGI << "Standard deviation: " << sstd << "us" << std::endl;

    KLOG("nuclear", 1) << "Parallel" << std::endl;
    for(size_t kk = 0; kk < nexp; ++kk)
    {
        std::fill(means.begin(), means.end(), 0.f);
        std::vector<JobSystem::JobHandle> handles;

        for(size_t ii = 0; ii < njobs; ++ii)
        {
            auto handle = js.schedule([&data, &means, ii]() {
                for(size_t jj = ii * len; jj < (ii + 1) * len; ++jj)
                {
                    means[ii] += data[jj];
                }
                means[ii] /= float(len);
            });
            handles.push_back(handle);
        }

        microClock clk;
        js.update();
        // std::this_thread::sleep_for(std::chrono::milliseconds(50));
        js.wait();

        float mean = 0.f;
        for(size_t ii = 0; ii < njobs; ++ii)
            mean += means[ii];
        mean /= float(njobs);
        durations[kk] = clk.get_elapsed_time().count();
        results[kk] = mean;
    }

    auto [pmean, pstd] = stats(durations);
    KLOGI << "Mean active time:   " << pmean << "us" << std::endl;
    KLOGI << "Standard deviation: " << pstd << "us" << std::endl;


    float gain_percent = 100.f * (smean - pmean) / smean;
    KLOG("nuclear",1) << "Gain: " << gain_percent << '%' << std::endl;

    return 0;
}