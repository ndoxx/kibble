#include "fmt/color.h"
#include "harness/job_example.h"

using namespace kb;

class JobExampleImpl : public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

/**
 * @brief In this example we demonstrate the use of barriers to create sync-points for a group of jobs.
 * We'll launch two groups of tasks, say update and render tasks, plus a few unrelated tasks in between.
 * Sometimes, update tasks will schedule a child job.
 * We'll then wait on the update and render barriers to ensure that the update and render tasks have completed.
 * You should see no update task message after the update barrier, and render task message after the render barrier.
 *
 * @param nexp number of experiments
 * @param njobs number of jobs
 * @param js job system instance
 * @param chan logger channel
 * @return int
 */
int JobExampleImpl::impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example 4] barriers");

    std::vector<long> load_time(njobs, 0l);
    random_fill(load_time.begin(), load_time.end(), 1l, 50l, 42);

    // Helper lambda to spawn a few unrelated tasks here and there
    auto spawn_unrelated_tasks = [&chan, &js](size_t num) {
        for (size_t ii = 0; ii < num; ++ii)
        {
            th::JobMetadata meta(th::WORKER_AFFINITY_ANY, "Unrelated");

            auto&& [tsk, fut] = js.create_task(std::move(meta), [&chan]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                klog(chan).debug("{}", fmt::styled("Unrelated", fmt::fg(fmt::color::blue_violet)));
            });

            // No barrier set here
            tsk.schedule();
        }
    };

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        klog(chan).info("Round #{}", kk);
        // Create barriers
        // Note that we could safely create the render_barrier later on, just before adding tasks to it
        th::barrier_t update_barrier = js.create_barrier();
        th::barrier_t render_barrier = js.create_barrier();

        spawn_unrelated_tasks(5);

        for (size_t ii = 0; ii < njobs; ++ii)
        {
            th::JobMetadata meta((ii < 70 ? th::WORKER_AFFINITY_ASYNC : th::WORKER_AFFINITY_ANY), "Update");

            auto&& [tsk, fut] = js.create_task(std::move(meta), [&load_time, &chan, ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
                klog(chan).debug("{} #{}", fmt::styled("Update", fmt::fg(fmt::color::yellow)), ii);
            });

            // Sometimes, add a child task
            if (ii % 3 == 0)
            {
                auto&& [child_tsk, child_fut] =
                    js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "Update"), [&load_time, &chan, ii]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii]));
                        klog(chan).debug("{} #{} (Child)", fmt::styled("Update", fmt::fg(fmt::color::yellow)), ii);
                    });
                // The child will inherit the update barrier automatically when it's scheduled
                tsk.add_child(child_tsk);
            }

            // Set up barrier: we'll be able to wait on it later on
            tsk.schedule(update_barrier);
        }

        spawn_unrelated_tasks(5);

        // Create a sync-point here, no update task can execute after it
        js.wait_on_barrier(update_barrier);
        // Now that the update barrier has been reached, we can safely destroy it
        js.destroy_barrier(update_barrier);
        klog(chan).info("Update sync-point reached");

        spawn_unrelated_tasks(5);

        for (size_t ii = 0; ii < njobs; ++ii)
        {
            th::JobMetadata meta(th::WORKER_AFFINITY_ANY, "Render");

            auto&& [tsk, fut] = js.create_task(std::move(meta), [&load_time, &chan, ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(load_time[ii] * 10));
                klog(chan).debug("{} #{}", fmt::styled("Render", fmt::fg(fmt::color::green)), ii);
            });

            tsk.schedule(render_barrier);
        }

        spawn_unrelated_tasks(20);

        // Create a sync-point here, no render task can execute after it
        js.wait_on_barrier(render_barrier);
        // Now that the render barrier has been reached, we can safely destroy it
        js.destroy_barrier(render_barrier);
        klog(chan).info("Render sync-point reached");
    }
    js.wait();

    return 0;
}