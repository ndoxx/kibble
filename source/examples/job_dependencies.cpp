#include "harness/job_example.h"
#include "assert/assert.h"

using namespace kb;

class JobExampleImpl : public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

/**
 * @brief In this example, we simulate multiple resource loading and staging jobs being dispatched to worker threads.
 * Each staging job depends on the loading job with same index. Staging jobs can only be performed by the main thread.
 * Some loading jobs can fail and throw an exception.
 *
 * @param nexp number of experiments
 * @param nloads number of loading tasks
 * @param js job system instance
 * @param chan logger channel
 * @return int
 */
int JobExampleImpl::impl(size_t nexp, size_t nloads, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example] mock async loading and staging");

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