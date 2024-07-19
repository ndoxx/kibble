#include "assert/assert.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "harness/job_example.h"
#include <mutex>
#include <unordered_set>

using namespace kb;

class JobExampleImpl : public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

/**
 * @brief Here, we show how a scheduled job can be preempted by main thread.
 *
 * @param nexp number of experiments
 * @param njobs number of loading tasks
 * @param js job system instance
 * @param chan logger channel
 * @return int
 */
int JobExampleImpl::impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example] job preemption");

    std::unordered_set<size_t> preempted;
    std::mutex mutex;

    size_t threshold = njobs - njobs / 10;
    for (size_t kk = 0; kk < nexp; ++kk)
    {
        preempted.clear();
        klog(chan).info("Round #{}", kk);

        std::vector<std::pair<size_t, th::Task>> premptible_tasks;
        for (size_t ii = 0; ii < njobs; ++ii)
        {
            th::JobMetadata meta(th::WORKER_AFFINITY_ANY, "Job");

            auto&& [tsk, fut] = js.create_task(std::move(meta), [ii, &chan, &preempted, &mutex]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                klog(chan).verbose("Job #{} executed", ii);

                // Make sure we did not execute a job that was preempted
                std::lock_guard lock(mutex);
                (void)preempted;
                K_ASSERT(!preempted.contains(ii), "Job was preempted but was executed by worker anyway: job_index={}",
                         ii);
            });

            tsk.schedule();

            // Keep track of jobs that can be preempted
            // This job is scheduled first, and preemption will probably fail
            if (ii == 0)
            {
                premptible_tasks.push_back({ii, std::move(tsk)});
            }

            // These jobs are scheduled last, and will likely be preempted
            if (ii >= threshold)
            {
                premptible_tasks.push_back({ii, std::move(tsk)});
            }
        }

        for (auto&& [idx, tsk] : premptible_tasks)
        {
            // Call this function to preempt a job and execute it here
            if (tsk.try_preempt_and_execute())
            {
                // The job was either idle or pending, and we managed to preempt it
                klog(chan).info("Job #{} {}", idx, fmt::styled("preempted", fmt::fg(fmt::color::yellow)));
                std::lock_guard lock(mutex);
                preempted.insert(idx);
            }
            else
            {
                // The job was executing or already processed, and we failed to preempt it
                klog(chan).warn("Job #{} premption failed", idx);
            }
        }
        js.wait();
    }

    return 0;
}