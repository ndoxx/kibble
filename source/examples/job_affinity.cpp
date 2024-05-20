#include "harness/job_example.h"

using namespace kb;

class JobExampleImpl : public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

/**
 * @brief In this example, we simulate multiple resource loading tasks being dispatched to worker threads. The loading
 * tasks are all independent.
 *
 * @param nexp number of experiments
 * @param nloads number of loading tasks
 * @param js job system instance
 * @param chan logger channel
 * @return int
 */
int JobExampleImpl::impl(size_t nexp, size_t nloads, kb::th::JobSystem& js, const kb::log::Channel& chan)
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