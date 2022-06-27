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

void show_statistics(milliClock &clk, long serial_dur_ms)
{
    auto parallel_dur_ms = clk.get_elapsed_time().count();
    float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
    float factor = float(serial_dur_ms) / float(parallel_dur_ms);
    KLOGI << "Estimated serial time: " << serial_dur_ms << "ms" << std::endl;
    KLOGI << "Parallel time:         " << parallel_dur_ms << "ms" << std::endl;
    KLOGI << "Factor:                " << (factor > 1 ? KS_POS_ : KS_NEG_) << factor << KC_ << std::endl;
    KLOGI << "Gain:                  " << (factor > 1 ? KS_POS_ : KS_NEG_) << gain_percent << KC_ << '%' << std::endl;
}

/**
 * @brief In this example, we simulate multiple resource loading jobs being dispatched to worker threads. The loading
 * jobs are all independent.
 *
 * @param nexp number of experiments
 * @param nloads number of loading jobs
 * @param scheme job system configuration
 * @param area initialized memory area
 * @return int
 */
int p0(size_t nexp, size_t nloads, const th::JobSystemScheme &scheme, memory::HeapArea &area)
{
    KLOGN("example") << "[JobSystem Example 0] mock async loading" << std::endl;

    th::JobSystem js(area, scheme);

    // A persistency file can be used to save job profile during this session, so the minimum load scheduler can
    // immediately use this information during the next run.
    js.use_persistence_file("p0.jpp");

    // We have nloads loading operations to execute asynchronously, each of them take a random amount of time
    std::vector<long> load_time(nloads, 0l);
    random_fill(load_time.begin(), load_time.end(), 1l, 100l, 42);

    // This is the time it would take to execute them all serially, so we have a baseline to compare the system's
    // performance to.
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);

    KLOG("example", 1) << "Assets loading time:" << std::endl;
    for (size_t ii = 0; ii < nloads; ++ii)
    {
        KLOGI << load_time[ii] << ' ';
    }
    KLOGI << std::endl;

    // We repeat the experiment nexp times
    for (size_t kk = 0; kk < nexp; ++kk)
    {
        KLOG("example", 1) << "Round " << kk << std::endl;

        // We save the job pointers in there so we can release them when we're done
        std::vector<th::Task<>> jobs;
        // Let's measure the total amount of time it takes to execute the tasks in parallel. Start the timer here so we
        // have an idea of the amount of job creation / scheduling overhead.
        milliClock clk;

        // Create as many jobs as needed
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            // Each job has some metadata attached
            th::JobMetadata meta;
            // A label uniquely identifies this job, so its execution time can be monitored
            meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            // A job's worker affinity property can be used to specify in which threads the job can or cannot be
            // executed. In this example, the first 70 (arbitrary) jobs must be executed asynchronously. The rest can be
            // executed on any thread, including the main thread
            if (ii < 70)
                meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;
            else
                meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            // Let's create a job and give it this simple lambda that waits a precise amount of time as a job kernel,
            // and also pass the metadata
            auto job = js.create_task(
                [&load_time, ii]() { std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii])); }, meta);

            // Schedule the job, the workers will awake
            job.schedule();
            // Save the job handle for later cleanup (or the job pool will leak)
            jobs.push_back(job);
        }
        // Wait for all jobs to be executed. This introduces a sync point. The main thread will assist the workers
        // instead of just waiting idly.
        js.wait();

        // Show some stats!
        show_statistics(clk, serial_dur_ms);

        // Cleanup
        for (auto &&job : jobs)
            job.release();
    }

    return 0;
}

/**
 * @brief Here we throw exceptions from job kernels and check that all is fine.
 *
 * @param njobs number of jobs
 * @param minload use minimum load scheduler
 * @param disable_work_stealing
 * @return int
 */
int p1(size_t njobs, const th::JobSystemScheme &scheme, memory::HeapArea &area)
{
    KLOGN("example") << "[JobSystem Example 1] throwing exceptions" << std::endl;

    th::JobSystem js(area, scheme);

    std::vector<th::Task<>> jobs;

    KLOG("example", 1) << "Creating jobs." << std::endl;

    // Create as many jobs as needed
    for (size_t ii = 0; ii < njobs; ++ii)
    {
        th::JobMetadata meta;
        meta.label = ii + 1;
        meta.worker_affinity = th::WORKER_AFFINITY_ANY;

        auto job = js.create_task(
            [ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if (ii % 40 == 0)
                    throw std::runtime_error("Runtime error!");
                else if (ii % 20 == 0)
                    throw std::logic_error("Logic error!");
            },
            meta);

        // Schedule the job, the workers will awake
        job.schedule();
        // Save the job pointer for later cleanup (or the job pool will leak)
        jobs.push_back(job);
    }
    js.wait();

    KLOG("example", 1) << "The exceptions should be rethrown now:" << std::endl;
    for (auto &&job : jobs)
    {
        try
        {
            job.release();
        }
        catch (std::exception &e)
        {
            KLOGE("example") << "Job #" << job.meta().label << " threw an exception: " << e.what() << std::endl;
        }
    }

    return 0;
}

/**
 * @brief In this example, we simulate multiple resource loading and staging jobs being dispatched to worker threads.
 * Each staging job depends on the loading job with same index. Staging jobs can only be performed by the main thread.
 * Some loading jobs can fail and throw an exception.
 *
 * @param nexp number of experiments
 * @param nloads number of loading jobs
 * @param scheme job system configuration
 * @param area initialized memory area
 * @return int
 */
int p2(size_t nexp, size_t nloads, const th::JobSystemScheme &scheme, memory::HeapArea &area)
{
    KLOGN("example") << "[JobSystem Example 2] mock async loading and staging" << std::endl;

    th::JobSystem js(area, scheme);
    js.use_persistence_file("p2.jpp");

    // In addition to loading tasks, we also simulate staging tasks (which take less time to complete)
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
        std::vector<th::Task<int>> load_jobs;
        std::vector<th::Task<float>> stage_jobs;
        milliClock clk;
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            // Create both jobs like we did in the first example
            th::JobMetadata load_meta;
            load_meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            if (ii < 70)
                load_meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;
            else
                load_meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto load_job = js.create_task<int>(
                [&load_time, ii, nloads](std::promise<int> &prom) {
                    // Simulate loading time
                    std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
                    // Sometimes, loading will fail and an exception will be thrown
                    if (ii == nloads / 2)
                        throw std::runtime_error("(Fake) Runtime error!");
                    // Don't forget to set the promise.
                    // For this trivial example we just produce a dummy integer.
                    prom.set_value(int(ii) * 2);
                },
                load_meta);

            // Staging jobs are executed on the main thread
            th::JobMetadata stage_meta;
            stage_meta.label = HCOMBINE_("Stage"_h, uint64_t(ii + 1));
            stage_meta.worker_affinity = th::WORKER_AFFINITY_MAIN;

            // Get the loading job future data so we can use it in the staging job
            auto fut = load_job.get_future();
            auto stage_job = js.create_task<float>(
                [&stage_time, ii, fut](std::promise<float> &prom) {
                    // Simulate staging time
                    std::this_thread::sleep_for(std::chrono::milliseconds(stage_time[ii]));
                    // Don't forget to set the promise.
                    // For this example, we just multiply by some arbitrary float...
                    prom.set_value(float(fut.get()) * 1.23f);
                },
                stage_meta);

            // But this time, we set the staging job as a child of the loading job. This means that the staging job will
            // not be scheduled until its parent loading job is complete. This makes sense in a real world situation:
            // first we need to load the resource from a file, then only can we upload it to OpenGL or whatever.
            load_job.add_child(stage_job);

            // We only schedule the parent job here, or we're asking for problems
            load_job.schedule();

            // Both job pointers must be freed when we're done
            load_jobs.push_back(load_job);
            stage_jobs.push_back(stage_job);
        }
        js.wait();

        // Gather some statistics
        show_statistics(clk, serial_dur_ms);

        // Release jobs and check end results
        for (auto &job : load_jobs)
            job.release();

        int ii = 0;
        for (auto &job : stage_jobs)
        {
            try
            {
                [[maybe_unused]] float val = job.get();
                // Check that the value is what we expect
                [[maybe_unused]] float expect = float(ii) * 2.f * 1.23f;
                [[maybe_unused]] constexpr float eps = 1e-10f;
                K_ASSERT(std::fabs(val - expect) < eps, "Value is not what we expect.");
            }
            catch (std::exception &e)
            {
                // If a loading job threw an exception, it will be rethrown on a call to fut.get() inside the
                // corresponding staging job kernel. So exceptions are forwarded down the promise pipe, and
                // we should catch them all right here.
                KLOGE("example") << "Job #" << job.meta().label << " threw an exception: " << e.what() << std::endl;
            }
            job.release();
            ++ii;
        }
    }

    return 0;
}

/**
 * @brief In this example, we submit a slightly more complex job graph to the job system.
 * Multiple job graphs will be submitted, and each have the following diamond shape:
 *
 *                                           B
 *                                         /   \
 *                                       A       D
 *                                         \   /
 *                                           D
 * So job 'A' needs to be executed first, then jobs 'B' and 'C' can run in parallel, and job
 * 'D' waits for both 'B' and 'C' to complete before it can run.
 *
 * @param nexp number of experiments
 * @param ngraphs number of loading jobs
 * @param scheme job system configuration
 * @param area initialized memory area
 * @return int
 */
int p3(size_t nexp, size_t ngraphs, const th::JobSystemScheme &scheme, memory::HeapArea &area)
{
    KLOGN("example") << "[JobSystem Example 3] mock async loading and staging" << std::endl;

    th::JobSystem js(area, scheme);
    js.use_persistence_file("p3.jpp");

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        KLOG("example", 1) << "Round " << kk << std::endl;
        std::vector<th::Task<int>> dep_jobs;
        std::vector<th::Task<bool>> end_jobs;
        milliClock clk;
        for (size_t ii = 0; ii < ngraphs; ++ii)
        {
            th::JobMetadata meta_a;
            meta_a.label = "A"_h;
            meta_a.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto job_a = js.create_task<int>(
                [ii](std::promise<int> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    prom.set_value(int(ii));
                },
                meta_a);

            th::JobMetadata meta_b;
            meta_b.label = "B"_h;
            meta_b.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto job_b = js.create_task<int>(
                [fut = job_a.get_future()](std::promise<int> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    prom.set_value(fut.get() * 2);
                },
                meta_b);

            th::JobMetadata meta_c;
            meta_c.label = "C"_h;
            meta_c.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto job_c = js.create_task<int>(
                [fut = job_a.get_future()](std::promise<int> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    prom.set_value(fut.get() * 3 - 10);
                },
                meta_c);

            th::JobMetadata meta_d;
            meta_d.label = "D"_h;
            meta_d.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto job_d = js.create_task<bool>(
                [fut_b = job_b.get_future(), fut_c = job_c.get_future()](std::promise<bool> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    prom.set_value(fut_b.get() < fut_c.get());
                },
                meta_d);

            job_b.add_parent(job_a);
            job_c.add_parent(job_a);
            job_b.add_child(job_d);
            job_c.add_child(job_d);

            job_a.schedule();

            dep_jobs.push_back(job_a);
            dep_jobs.push_back(job_b);
            dep_jobs.push_back(job_c);
            end_jobs.push_back(job_d);
        }
        js.wait();

        long estimated_serial_time_ms = long(ngraphs) * (5 + 10 + 15 + 5);
        show_statistics(clk, estimated_serial_time_ms);

        // Release jobs and check end results
        for (auto &job : dep_jobs)
            job.release();

        int ii = 0;
        for (auto &job : end_jobs)
        {
            [[maybe_unused]] bool val = job.get();
            // Check that the value is what we expect
            [[maybe_unused]] bool expect = 2 * ii < 3 * ii - 10;
            K_ASSERT(val == expect, "Value is not what we expect.");

            job.release();
            ++ii;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    init_logger();

    ap::ArgParse parser("job_system_example", "0.1");
    parser.set_log_output([](const std::string &str) { KLOG("kibble", 1) << str << std::endl; });
    const auto &ex = parser.add_positional<int>("EXAMPLE", "Select the example function to run in [0,3]");
    const auto &ne = parser.add_variable<int>('e', "experiments", "Number of experiments to perform", 4);
    const auto &nj = parser.add_variable<int>('j', "jobs", "Number of jobs", 100);
    const auto &ml = parser.add_flag('m', "minload", "Use minimal load scheduler");
    const auto &WS = parser.add_flag('W', "disable-work-stealing", "Disable the work stealing feature");

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser);

    size_t nexp = std::min(size_t(ne()), 100ul);
    size_t njob = std::min(size_t(nj()), 500ul);

    // First, we create a scheme to configure the job system
    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.enable_work_stealing = !WS();
    scheme.max_stealing_attempts = 16;
    scheme.scheduling_algorithm = ml() ? th::SchedulingAlgorithm::min_load : th::SchedulingAlgorithm::round_robin;

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    memory::HeapArea area(th::JobSystem::get_memory_requirements());

    // clang-format off
    switch(ex())
    {
        case 0: return p0(nexp, njob, scheme, area);
        case 1: return p1(njob, scheme, area);
        case 2: return p2(nexp, njob, scheme, area);
        case 3: return p3(nexp, njob, scheme, area);
        default: 
        {
            KLOGW("example") << "Unknown example: " << ex() << std::endl;
            return 0;
        }
    }
    // clang-format on
}