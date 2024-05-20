#include "harness/job_example.h"

using namespace kb;

class JobExampleImpl : public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

/**
 * @brief Here we throw exceptions from job kernels and check that all is fine.
 *
 * @param ntasks number of jobs
 * @param js job system instance
 * @param chan logger channel
 * @return int
 */
int JobExampleImpl::impl(size_t nexp, size_t ntasks, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    (void)nexp;
    klog(chan).info("[JobSystem Example] throwing exceptions");
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