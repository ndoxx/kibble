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
 * @brief In this example, we submit a slightly more complex job graph to the job system.
 * Multiple job graphs will be submitted, and each have the following diamond shape:
 *
 *                                           B
 *                                         /   \
 *                                       A       D
 *                                         \   /
 *                                           C
 * So job 'A' needs to be executed first, then jobs 'B' and 'C' can run in parallel, and job
 * 'D' waits for both 'B' and 'C' to complete before it can run.
 *
 * @param nexp number of experiments
 * @param ngraphs number of loading jobs
 * @param js job system instance
 * @param chan logger channel
 * @return int
 */
int JobExampleImpl::impl(size_t nexp, size_t ngraphs, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    klog(chan).info("[JobSystem Example] diamond graphs");

    for (size_t kk = 0; kk < nexp; ++kk)
    {
        klog(chan).info("Round #{}", kk);
        std::vector<std::shared_future<bool>> end_futs;
        milliClock clk;
        for (size_t ii = 0; ii < ngraphs; ++ii)
        {
            auto&& [tsk_a, fut_a] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "A"), [ii]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                // fmt::println("Task A #{}", ii);
                return int(ii);
            });

            // We could pass futures as function arguments like previously
            // But lambda capture also works
            auto&& [tsk_b, fut_b] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "B"), [ii, fut = fut_a]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                // fmt::println("Task B #{}: {}", ii, fut.get());
                (void)ii;
                return fut.get() * 2;
            });

            auto&& [tsk_c, fut_c] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "C"), [ii, fut = fut_a]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                // fmt::println("Task C #{}: {}", ii, fut.get());
                (void)ii;
                return fut.get() * 3 - 10;
            });

            auto&& [tsk_d, fut_d] =
                js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "D"), [ii, fut_1 = fut_b, fut_2 = fut_c]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    // fmt::println("Task D #{}: {} {}", ii, fut_1.get(), fut_2.get());
                    (void)ii;
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
            K_ASSERT(val == expect, "Value is not what we expect.");

            ++ii;
        }
    }

    return 0;
}