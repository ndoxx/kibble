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

    KLOGGER(create_channel("example", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("thread", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void show_error_and_die(ap::ArgParse &parser)
{
    for (const auto &msg : parser.get_errors())
        KLOGW("kibble") << msg << std::endl;

    KLOG("kibble", 1) << parser.usage() << std::endl;
    exit(0);
}

std::pair<float, float> stats(const std::vector<long> durations)
{
    float mu = float(std::accumulate(durations.begin(), durations.end(), 0)) / float(durations.size());
    float variance = 0.f;
    for (long d : durations)
        variance += (float(d) - mu) * (float(d) - mu);
    variance /= float(durations.size());
    return {mu, std::sqrt(variance)};
}

template <typename T, typename Iter>
void random_fill(Iter start, Iter end, T min, T max, uint64_t seed = 0)
{
    static std::default_random_engine eng(seed);
    std::uniform_int_distribution<T> dist(min, max);
    std::generate(start, end, [&]() { return dist(eng); });
}

/**
 * @brief In this example, we simulate multiple resource loading jobs being dispatched to worker threads. The loading
 * jobs are all independent.
 *
 * @param nexp number of experiments
 * @param nloads number of loading jobs
 * @param minload use minimum load scheduler
 * @param disable_work_stealing
 * @return int
 */
int p0(size_t nexp, size_t nloads, bool minload, bool disable_work_stealing)
{
    KLOGN("example") << "[JobSystem Example 0] mock async loading" << std::endl;

    // First, we create a scheme to configure the job system
    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.enable_work_stealing = !disable_work_stealing;
    scheme.scheduling_algorithm = minload ? th::SchedulingAlgorithm::min_load : th::SchedulingAlgorithm::round_robin;

    memory::HeapArea area(1_MB);
    th::JobSystem js(area, scheme);
    js.use_persistence_file("p1.jpp");

    std::vector<long> load_time(nloads, 0l);
    random_fill(load_time.begin(), load_time.end(), 1l, 100l, 42);
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);

    KLOG("example", 1) << "Assets loading time:" << std::endl;
    for (size_t ii = 0; ii < nloads; ++ii)
    {
        KLOGI << load_time[ii] << ' ';
    }
    KLOGI << std::endl;

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        KLOG("example", 1) << "Round " << kk << std::endl;
        std::vector<th::Job *> jobs;
        milliClock clk;
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            th::JobMetadata meta;
            meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            if (ii < 70)
                meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;
            else
                meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto *job = js.create_job(
                [&load_time, ii]() { std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii])); }, meta);
            js.schedule(job);
            jobs.push_back(job);
        }
        js.wait();
        auto parallel_dur_ms = clk.get_elapsed_time().count();

        float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
        float factor = float(serial_dur_ms) / float(parallel_dur_ms);
        KLOGI << "Estimated serial time: " << serial_dur_ms << "ms" << std::endl;
        KLOGI << "Parallel time:         " << parallel_dur_ms << "ms" << std::endl;
        KLOGI << "Factor:                " << (factor > 1 ? KS_GOOD_ : KS_BAD_) << factor << KC_ << std::endl;
        KLOGI << "Gain:                  " << (factor > 1 ? KS_GOOD_ : KS_BAD_) << gain_percent << KC_ << '%'
              << std::endl;

        for (auto *job : jobs)
            js.release_job(job);
    }

    return 0;
}

/**
 * @brief In this example, we simulate multiple resource loading and staging jobs being dispatched to worker threads.
 * Each staging job depends on the loading job with same index. Staging jobs can only be performed by the main thread.
 *
 * @param nexp number of experiments
 * @param nloads number of loading jobs
 * @param minload use minimum load scheduler
 * @param disable_work_stealing
 * @return int
 */
int p1(size_t nexp, size_t nloads, bool minload, bool disable_work_stealing)
{
    KLOGN("example") << "[JobSystem Example 1] mock async loading and staging" << std::endl;

    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.enable_work_stealing = !disable_work_stealing;
    scheme.max_stealing_attempts = 16;
    scheme.scheduling_algorithm = minload ? th::SchedulingAlgorithm::min_load : th::SchedulingAlgorithm::round_robin;

    memory::HeapArea area(1_MB);
    th::JobSystem js(area, scheme);
    js.use_persistence_file("p2.jpp");

    std::vector<long> load_time(nloads, 0l);
    std::vector<long> stage_time(nloads, 0l);
    random_fill(load_time.begin(), load_time.end(), 1l, 100l, 42);
    random_fill(stage_time.begin(), stage_time.end(), 1l, 10l, 42);
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);
    serial_dur_ms += std::accumulate(stage_time.begin(), stage_time.end(), 0l);

    KLOG("example", 1) << "Assets loading / staging time:" << std::endl;
    for (size_t ii = 0; ii < nloads; ++ii)
    {
        KLOGI << load_time[ii] << '/' << stage_time[ii] << ' ';
    }
    KLOGI << std::endl;

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        KLOG("example", 1) << "Round " << kk << std::endl;
        std::vector<th::Job *> jobs;
        milliClock clk;
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            th::JobMetadata load_meta;
            load_meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            if (ii < 70)
                load_meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;
            else
                load_meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto *load_job = js.create_job(
                [&load_time, ii]() { std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii])); },
                load_meta);

            th::JobMetadata stage_meta;
            stage_meta.label = HCOMBINE_("Stage"_h, uint64_t(ii + 1));
            stage_meta.worker_affinity = th::WORKER_AFFINITY_MAIN;

            auto *stage_job = js.create_job(
                [&stage_time, ii]() { std::this_thread::sleep_for(std::chrono::milliseconds(stage_time[ii])); },
                stage_meta);

            load_job->add_child(stage_job);

            js.schedule(load_job);
            jobs.push_back(load_job);
            jobs.push_back(stage_job);
        }
        js.wait();
        auto parallel_dur_ms = clk.get_elapsed_time().count();

        float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
        float factor = float(serial_dur_ms) / float(parallel_dur_ms);
        KLOGI << "Estimated serial time: " << serial_dur_ms << "ms" << std::endl;
        KLOGI << "Parallel time:         " << parallel_dur_ms << "ms" << std::endl;
        KLOGI << "Factor:                " << (factor > 1 ? KS_GOOD_ : KS_BAD_) << factor << KC_ << std::endl;
        KLOGI << "Gain:                  " << (factor > 1 ? KS_GOOD_ : KS_BAD_) << gain_percent << KC_ << '%'
              << std::endl;

        for (auto *job : jobs)
            js.release_job(job);
    }

    return 0;
}

int main(int argc, char **argv)
{
    init_logger();

    ap::ArgParse parser("job_system_example", "0.1");
    parser.set_log_output([](const std::string &str) { KLOG("kibble", 1) << str << std::endl; });
    const auto &ex = parser.add_positional<int>("EXAMPLE", "Select the example function to run in [0,1]");
    const auto &ne = parser.add_variable<int>('e', "experiments", "Number of experiments to perform", 4);
    const auto &nj = parser.add_variable<int>('j', "jobs", "Number of jobs", 100);
    const auto &ml = parser.add_flag('m', "minload", "Use minimal load scheduler");
    const auto &WS = parser.add_flag('W', "disable-work-stealing", "Disable the work stealing feature");

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser);

    size_t nexp = std::min(size_t(ne()), 10ul);
    size_t njob = std::min(size_t(nj()), 500ul);

    if (ex() == 0)
        return p0(nexp, njob, ml(), WS());
    else
        return p1(nexp, njob, ml(), WS());
}