#include "harness/job_example.h"
#include "thread/job/daemon.h"

using namespace kb;

class JobExampleImpl : public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

int JobExampleImpl::impl(size_t nframes, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    th::DaemonScheduler ds(js);

    // Data for the daemons
    struct Message
    {
        std::string message;
        float interval_ms;
        int64_t ttl = 0;
    };

    // Daemons with a default ttl of 0 will run till they are killed.
    // The second one has a ttl of 4 and will only execute 4 times.
    // The cooldown_ms property could also be initialized with a positive
    // value to delay execution.
    std::vector<Message> msgs = {{"hello", 100.f}, {"salut", 200.f, 4}, {"sunt eu", 500.f}, {"un haiduc", 1000.f}};
    std::vector<th::DaemonHandle> hnds;

    // Launch daemons
    for (const auto& msg : msgs)
    {
        // Daemons will just log a message and wait a bit
        auto hnd = ds.create(
            [msg, &chan]() {
                klog(chan).uid("Daemon").info(msg.message);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return true;
            },
            th::SchedulingData{.interval_ms = msg.interval_ms, .ttl = msg.ttl},
            th::JobMetadata(th::WORKER_AFFINITY_ASYNC, msg.message));
        // interval_ms: Sets the interval in ms at which the tasks will be rescheduled
        // ttl:         Sets the number of repetitions

        // Save handles, in case we need to kill daemons while they're running
        hnds.push_back(hnd);
    }

    // Simulate game loop
    for (size_t ii = 0; ii < nframes; ++ii)
    {
        microClock clk;
        // Call this function each frame so daemons can be rescheduled
        ds.update();

        // Create a few independent tasks each frame
        for (size_t jj = 0; jj < size_t(njobs); ++jj)
        {
            auto&& [tsk, fut] = js.create_task(th::JobMetadata(th::WORKER_AFFINITY_ANY, "job"),
                                               []() { std::this_thread::sleep_for(std::chrono::microseconds(500)); });
            tsk.schedule();
        }

        // Kill the first daemon manually after some time
        if (ii == nframes / 2)
            ds.kill(hnds[0]);

        // Wait on all jobs to finish
        js.wait();
    }

    return 0;
}