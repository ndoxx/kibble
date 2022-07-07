#include "argparse/argparse.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job/job_system.h"
#include "time/clock.h"
#include "time/instrumentation.h"

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
 * @brief In this example, we simulate multiple resource loading tasks being dispatched to worker threads. The loading
 * tasks are all independent.
 *
 * @param nexp number of experiments
 * @param nloads number of loading tasks
 * @param scheme job system configuration
 * @param area initialized memory area
 * @return int
 */
int p0(size_t nexp, size_t nloads, th::JobSystem &js)
{
    KLOGN("example") << "[JobSystem Example 0] mock async loading" << std::endl;

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

        // Let's measure the total amount of time it takes to execute the tasks in parallel. Start the timer here so we
        // have an idea of the amount of task creation / scheduling overhead.
        milliClock clk;

        // Create as many tasks as needed
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            // Each task has some metadata attached
            th::JobMetadata meta;
            // A name for profiling
            meta.set_profile_data("Load");
            // A label uniquely identifies this task, so its execution time can be monitored
            meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            // A job's worker affinity property can be used to specify in which threads the job can or cannot be
            // executed. In this example, the first 70 (arbitrary) jobs must be executed asynchronously. The rest can be
            // executed on any thread, including the main thread
            if (ii < 70)
                meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;
            else
                meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            // Let's create a task and give it this simple lambda that waits a precise amount of time as a job kernel,
            // and also pass the metadata
            auto tsk = js.create_task(
                [&load_time, ii]() { std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii])); }, meta);

            // Schedule the tsk, the workers will awake
            tsk.schedule();
        }
        // Wait for all jobs to be executed. This introduces a sync point. The main thread will assist the workers
        // instead of just waiting idly.
        js.wait();

        // Show some stats!
        show_statistics(clk, serial_dur_ms);
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
int p1(size_t ntasks, th::JobSystem &js)
{
    KLOGN("example") << "[JobSystem Example 1] throwing exceptions" << std::endl;

    KLOG("example", 1) << "Creating tasks." << std::endl;

    // Create as many tasks as needed
    for (size_t ii = 0; ii < ntasks; ++ii)
    {
        th::JobMetadata meta;
        meta.set_profile_data("MyTask");
        meta.label = ii + 1;
        meta.worker_affinity = th::WORKER_AFFINITY_ANY;

        auto tsk = js.create_task(
            [ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if (ii % 40 == 0)
                    throw std::runtime_error("Runtime error!");
                else if (ii % 20 == 0)
                    throw std::logic_error("Logic error!");
            },
            meta);

        // Schedule the task, the workers will awake
        tsk.schedule();
    }
    KLOG("example", 1) << "The exceptions should be rethrown now:" << std::endl;
    js.wait();

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
int p2(size_t nexp, size_t nloads, th::JobSystem &js)
{
    KLOGN("example") << "[JobSystem Example 2] mock async loading and staging" << std::endl;

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
        std::vector<th::Task<float>> stage_tasks;
        milliClock clk;
        for (size_t ii = 0; ii < nloads; ++ii)
        {
            // Create both tasks like we did in the first example
            th::JobMetadata load_meta;
            load_meta.set_profile_data("Load");
            load_meta.label = HCOMBINE_("Load"_h, uint64_t(ii + 1));
            if (ii < 70)
                load_meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;
            else
                load_meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto load_task = js.create_task<int>(
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
            stage_meta.set_profile_data("Stage");
            stage_meta.label = HCOMBINE_("Stage"_h, uint64_t(ii + 1));
            stage_meta.worker_affinity = th::WORKER_AFFINITY_MAIN;

            // Get the loading task future data so we can use it in the staging task
            auto fut = load_task.get_future();
            auto stage_task = js.create_task<float>(
                [&stage_time, ii, fut](std::promise<float> &prom) {
                    // Simulate staging time
                    std::this_thread::sleep_for(std::chrono::milliseconds(stage_time[ii]));
                    // Don't forget to set the promise.
                    // For this example, we just multiply by some arbitrary float...
                    prom.set_value(float(fut.get()) * 1.23f);
                },
                stage_meta);

            // But this time, we set the staging task as a child of the loading task. This means that the staging job
            // will not be scheduled until its parent loading job is complete. This makes sense in a real world
            // situation: first we need to load the resource from a file, then only can we upload it to OpenGL or
            // whatever.
            load_task.add_child(stage_task);

            // We only schedule the parent task here, or we're asking for problems
            load_task.schedule();

            // Keep staging tasks so we can check their results
            stage_tasks.push_back(stage_task);
        }
        js.wait();

        // Gather some statistics
        show_statistics(clk, serial_dur_ms);

        int ii = 0;
        for (auto &tsk : stage_tasks)
        {
            try
            {
                [[maybe_unused]] float val = tsk.get();
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
                KLOGE("example") << "Job #" << tsk.meta().label << " threw an exception: " << e.what() << std::endl;
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
int p3(size_t nexp, size_t ngraphs, th::JobSystem &js)
{
    KLOGN("example") << "[JobSystem Example 3] diamond graphs" << std::endl;

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        KLOG("example", 1) << "Round " << kk << std::endl;
        std::vector<th::Task<bool>> end_tasks;
        milliClock clk;
        for (size_t ii = 0; ii < ngraphs; ++ii)
        {
            th::JobMetadata meta_a;
            meta_a.set_profile_data("A");
            meta_a.label = "A"_h;
            meta_a.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto tsk_a = js.create_task<int>(
                [ii](std::promise<int> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    prom.set_value(int(ii));
                },
                meta_a);

            th::JobMetadata meta_b;
            meta_b.set_profile_data("B");
            meta_b.label = "B"_h;
            meta_b.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto tsk_b = js.create_task<int>(
                [fut = tsk_a.get_future()](std::promise<int> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    prom.set_value(fut.get() * 2);
                },
                meta_b);

            th::JobMetadata meta_c;
            meta_c.set_profile_data("C");
            meta_c.label = "C"_h;
            meta_c.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto tsk_c = js.create_task<int>(
                [fut = tsk_a.get_future()](std::promise<int> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    prom.set_value(fut.get() * 3 - 10);
                },
                meta_c);

            th::JobMetadata meta_d;
            meta_d.set_profile_data("D");
            meta_d.label = "D"_h;
            meta_d.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto tsk_d = js.create_task<bool>(
                [fut_b = tsk_b.get_future(), fut_c = tsk_c.get_future()](std::promise<bool> &prom) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    prom.set_value(fut_b.get() < fut_c.get());
                },
                meta_d);

            tsk_b.add_parent(tsk_a);
            tsk_c.add_parent(tsk_a);
            tsk_b.add_child(tsk_d);
            tsk_c.add_child(tsk_d);

            tsk_a.schedule();

            end_tasks.push_back(tsk_d);
        }
        js.wait();

        long estimated_serial_time_ms = long(ngraphs) * (5 + 10 + 15 + 5);
        show_statistics(clk, estimated_serial_time_ms);

        int ii = 0;
        for (auto &tsk : end_tasks)
        {
            [[maybe_unused]] bool val = tsk.get();
            // Check that the value is what we expect
            [[maybe_unused]] bool expect = 2 * ii < 3 * ii - 10;
            K_ASSERT(val == expect, "Value is not what we expect.");

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

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser);

    size_t nexp = std::min(size_t(ne()), 100ul);
    size_t njob = std::min(size_t(nj()), 500ul);

    // First, we create a scheme to configure the job system
    using namespace std::literals::string_literals;

    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.max_stealing_attempts = 16;

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    memory::HeapArea area(th::JobSystem::get_memory_requirements());

    auto *js = new th::JobSystem(area, scheme);

    // Job system profiling (compile )
    auto *session = new InstrumentationSession();
    js->set_instrumentation_session(session);

    // clang-format off
    int ret = 0;
    switch(ex())
    {
        case 0: ret = p0(nexp, njob, *js); break;
        case 1: ret = p1(njob, *js); break;
        case 2: ret = p2(nexp, njob, *js); break;
        case 3: ret = p3(nexp, njob, *js); break;
        default: 
        {
            KLOGW("example") << "Unknown example: " << ex() << std::endl;
        }
    }
    // clang-format on

    delete js;

    session->write("p"s + std::to_string(ex()) + "_profile.json");
    delete session;
    return ret;
}