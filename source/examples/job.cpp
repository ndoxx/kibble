#define K_DEBUG
#include "argparse/argparse.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job/job_system.h"
#include "time/clock.h"
#include "time/instrumentation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "fmt/std.h"
#include <iterator>
#include <numeric>
#include <random>
#include <regex>
#include <string>
#include <vector>

using namespace kb;
using namespace kb::log;

void show_error_and_die(ap::ArgParse& parser, const Channel& chan)
{
    for (const auto& msg : parser.get_errors())
        klog(chan).warn(msg);

    klog(chan).raw().info(parser.usage());
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

void show_statistics(milliClock& clk, long serial_dur_ms, const kb::log::Channel& chan)
{
    auto parallel_dur_ms = clk.get_elapsed_time().count();
    float gain_percent = 100.f * float(parallel_dur_ms - serial_dur_ms) / float(serial_dur_ms);
    float factor = float(serial_dur_ms) / float(parallel_dur_ms);
    klog(chan).verbose("Estimated serial time: {}ms", serial_dur_ms);
    klog(chan).verbose("Parallel time:         {}ms", parallel_dur_ms);
    klog(chan).verbose("Factor:                {}", factor);
    klog(chan).verbose("Gain:                  {}%", gain_percent);
}

/**
 * @brief In this example, we simulate multiple resource loading tasks being dispatched to worker threads. The loading
 * tasks are all independent.
 *
 * @param nexp number of experiments
 * @param nloads number of loading tasks
 * @param scheme job system configuration
 * @param area initialized memory area
 * @return int
 */
int p0(size_t nexp, size_t nloads, th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example 0] mock async loading");

    // We have nloads loading operations to execute asynchronously, each of them take a random amount of time
    std::vector<long> load_time(nloads, 0l);
    random_fill(load_time.begin(), load_time.end(), 1l, 100l, 42);

    // This is the time it would take to execute them all serially, so we have a baseline to compare the system's
    // performance to.
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);

    klog(chan).verbose("Asset loading times:");
    for (long lt : load_time)
    {
        klog(chan).verbose("{}", lt);
    }

    // We repeat the experiment nexp times
    for (size_t kk = 0; kk < nexp; ++kk)
    {
        klog(chan).info("Round #{}", kk);

        // Let's measure the total amount of time it takes to execute the tasks in parallel. Start the timer here so we
        // have an idea of the amount of task creation / scheduling overhead.
        milliClock clk;

        // Create as many tasks as needed
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            // Each task has some metadata attached
            // A job's worker affinity property can be used to specify in which threads the job can or cannot be
            // executed. In this example, the first 70 (arbitrary) jobs must be executed asynchronously. The rest can be
            // executed on any thread, including the main thread
            // Also provide a name for profiling
            th::JobMetadata meta((ii < 70 ? th::WORKER_AFFINITY_ASYNC : th::WORKER_AFFINITY_ANY), "Load");

            // Let's create a task and give it this simple lambda that waits a precise amount of time as a job kernel,
            // and also pass the metadata
            // Note that the create_task() function also returns a (shared) future, more on that later.
            auto&& [tsk, fut] = js.create_task(std::move(meta), [&load_time, ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
            });

            // Schedule the tsk, the workers will awake
            tsk.schedule();
        }
        // Wait for all jobs to be executed. This introduces a sync point. The main thread will assist the workers
        // instead of just waiting idly.
        js.wait();

        // Show some stats!
        show_statistics(clk, serial_dur_ms, chan);
    }

    return 0;
}

/**
 * @brief Here we throw exceptions from job kernels and check that all is fine.
 *
 * @param ntasks number of jobs
 * @param minload use minimum load scheduler
 * @param disable_work_stealing
 * @return int
 */
int p1(size_t ntasks, th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example 1] throwing exceptions");
    klog(chan).info("Creating tasks.");

    // Create as many tasks as needed
    // Some of these tasks will throw an exception
    std::vector<std::shared_future<void>> futs;
    for (size_t ii = 0; ii < ntasks; ++ii)
    {
        auto&& [tsk, fut] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "MyTask"), [ii]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (ii % 40 == 0)
                throw std::runtime_error("(Fake) Runtime error!");
            else if (ii % 20 == 0)
                throw std::logic_error("(Fake) Logic error!");
        });

        // Schedule the task, the workers will awake
        tsk.schedule();
        // This time we keep the futures, because we're going to wait on them
        futs.push_back(fut);
    }

    // If a task throws an exception, it is captured in the future and rethrown on a
    // call to future.get()
    klog(chan).info("The exceptions should be rethrown now:");
    for (auto&& fut : futs)
    {
        try
        {
            fut.get();
        }
        catch (std::exception& e)
        {
            klog(chan).error(e.what());
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
 * @param nloads number of loading tasks
 * @param scheme job system configuration
 * @param area initialized memory area
 * @return int
 */
int p2(size_t nexp, size_t nloads, th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example 2] mock async loading and staging");

    // In addition to loading tasks, we also simulate staging tasks (which take less time to complete)
    std::vector<long> load_time(nloads, 0l);
    std::vector<long> stage_time(nloads, 0l);
    random_fill(load_time.begin(), load_time.end(), 1l, 100l, 42);
    random_fill(stage_time.begin(), stage_time.end(), 1l, 10l, 42);
    long serial_dur_ms = std::accumulate(load_time.begin(), load_time.end(), 0l);
    serial_dur_ms += std::accumulate(stage_time.begin(), stage_time.end(), 0l);

    klog(chan).verbose("Assets loading / staging time:");
    for (size_t ii = 0; ii < nloads; ++ii)
    {
        klog(chan).verbose("{} / {}", load_time[ii], stage_time[ii]);
    }

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        klog(chan).verbose("Round #", kk);
        std::vector<std::shared_future<float>> stage_futs;
        milliClock clk;
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            // Create both tasks like we did in the first example
            th::JobMetadata load_meta((ii < 70 ? th::WORKER_AFFINITY_ASYNC : th::WORKER_AFFINITY_ANY), "Load");

            auto [load_task, load_fut] = js.create_task(std::move(load_meta), [&load_time, ii, nloads]() {
                // Simulate loading time
                std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
                // Sometimes, loading will fail and an exception will be thrown
                if (ii == nloads / 2)
                    throw std::runtime_error("(Fake) Runtime error!");
                // For this trivial example we just produce a dummy integer.
                return int(ii) * 2;
            });

            // Get the loading task future data so we can use it in the staging task
            // Staging jobs are executed on the main thread
            // The future result of the loading task is passed as a function argument
            // We could also use lambda capture for that, see the next example
            auto&& [stage_task, stage_fut] = js.create_task(
                th::JobMetadata(th::WORKER_AFFINITY_MAIN, "Stage"),
                [&stage_time, ii](std::shared_future<int> fut) {
                    // Simulate staging time
                    std::this_thread::sleep_for(std::chrono::milliseconds(stage_time[ii]));
                    // For this example, we just multiply by some arbitrary float...
                    return float(fut.get()) * 1.23f;
                },
                load_fut);

            // But this time, we set the staging task as a child of the loading task. This means that the staging job
            // will not be scheduled until its parent loading job is complete. This makes sense in a real world
            // situation: first we need to load the resource from a file, then only can we upload it to OpenGL or
            // whatever.
            load_task.add_child(stage_task);

            // We only schedule the parent task here, or we're asking for problems
            load_task.schedule();

            // Keep staging futures so we can check their results
            stage_futs.push_back(stage_fut);
        }
        js.wait();

        // Gather some statistics
        show_statistics(clk, serial_dur_ms, chan);

        int ii = 0;
        for (auto& fut : stage_futs)
        {
            try
            {
                [[maybe_unused]] float val = fut.get();
                // Check that the value is what we expect
                [[maybe_unused]] float expect = float(ii) * 2.f * 1.23f;
                [[maybe_unused]] constexpr float eps = 1e-10f;
                K_ASSERT(std::fabs(val - expect) < eps, "Value is not what we expect.", &chan);
            }
            catch (std::exception& e)
            {
                // If a loading job threw an exception, it will be rethrown on a call to fut.get() inside the
                // corresponding staging job kernel. So exceptions are forwarded down the promise pipe, and
                // we should catch them all right here.
                klog(chan).error("A job threw an exception:\n{}", e.what());
            }
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
int p3(size_t nexp, size_t ngraphs, th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example 3] diamond graphs");

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        klog(chan).info("Round #{}", kk);
        std::vector<std::shared_future<bool>> end_futs;
        milliClock clk;
        for (size_t ii = 0; ii < ngraphs; ++ii)
        {
            auto&& [tsk_a, fut_a] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "A"), [ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                return int(ii);
            });

            // We could pass futures as function arguments like previously
            // But lambda capture also works
            auto&& [tsk_b, fut_b] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "B"), [fut = fut_a]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return fut.get() * 2;
            });

            auto&& [tsk_c, fut_c] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "C"), [fut = fut_a]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                return fut.get() * 3 - 10;
            });

            auto&& [tsk_d, fut_d] =
                js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "D"), [fut_1 = fut_b, fut_2 = fut_c]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    return fut_1.get() < fut_2.get();
                });

            tsk_b.add_parent(tsk_a);
            tsk_c.add_parent(tsk_a);
            tsk_b.add_child(tsk_d);
            tsk_c.add_child(tsk_d);

            tsk_a.schedule();

            end_futs.push_back(fut_d);
        }
        js.wait();

        long estimated_serial_time_ms = long(ngraphs) * (5 + 10 + 15 + 5);
        show_statistics(clk, estimated_serial_time_ms, chan);

        int ii = 0;
        for (auto& fut : end_futs)
        {
            [[maybe_unused]] bool val = fut.get();
            // Check that the value is what we expect
            [[maybe_unused]] bool expect = 2 * ii < 3 * ii - 10;
            K_ASSERT(val == expect, "Value is not what we expect.", &chan);

            ++ii;
        }
    }

    return 0;
}

int main(int argc, char** argv)
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
    const auto& ex = parser.add_positional<int>("EXAMPLE", "Select the example function to run in [0,3]");
    const auto& ne = parser.add_variable<int>('e', "experiments", "Number of experiments to perform", 4);
    const auto& nj = parser.add_variable<int>('j', "jobs", "Number of jobs", 100);

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan_kibble);

    size_t nexp = std::min(size_t(ne()), 100ul);
    size_t njob = std::min(size_t(nj()), 500ul);

    // First, we create a scheme to configure the job system
    using namespace std::literals::string_literals;

    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.max_stealing_attempts = 16;

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    memory::HeapArea area(th::JobSystem::get_memory_requirements(), &chan_memory);

    auto* js = new th::JobSystem(area, scheme, &chan_thread);
    Channel::set_async(js);

    // Job system profiling
    auto* session = new InstrumentationSession();
    js->set_instrumentation_session(session);

    // clang-format off
    int ret = 0;
    switch(ex())
    {
        case 0: ret = p0(nexp, njob, *js, chan_kibble); break;
        case 1: ret = p1(njob, *js, chan_kibble); break;
        case 2: ret = p2(nexp, njob, *js, chan_kibble); break;
        case 3: ret = p3(nexp, njob, *js, chan_kibble); break;
        default: 
        {
            klog(chan_kibble).warn("Unknown example: {}", ex());
        }
    }
    // clang-format on

    delete js;

    session->write("p"s + std::to_string(ex()) + "_profile.json");
    delete session;
    return ret;
}