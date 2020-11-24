#include "logger/logger.h"
#include "logger/logger_sink.h"
#include "logger/logger_thread.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job.h"
#include "time/clock.h"

#include <regex>
#include <string>
#include <vector>
#include <numeric>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("thread", 2));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_single_threaded(true));
    KLOGGER(set_backtrace_on_error(false));
    KLOGGER(spawn());
    KLOGGER(sync());
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    init_logger();

    KLOGN("nuclear") << "Start" << std::endl;

    memory::HeapArea area(512_kB);
    JobSystem js(area);

    // std::vector<float> experiments;

    constexpr size_t len = 256;
    constexpr size_t njobs = 128;

    microClock clk;
    for(size_t kk=0; kk<1000; ++kk)
    {
        std::vector<float> data(njobs*len);
        std::iota(data.begin(), data.end(), 0.f);
        std::array<float,njobs> means;
        std::fill(means.begin(), means.end(), 0.f);
        std::vector<JobSystem::JobHandle> handles;

        for(size_t ii=0; ii<njobs; ++ii)
        {
            auto handle = js.schedule([&data,&means,ii]()
            {
                for(size_t jj=ii*len; jj<(ii+1)*len; ++jj)
                {
                    means[ii] += data[jj];
                }
                means[ii] /= float(len);
            });
            handles.push_back(handle);
        }


        js.update();
        // std::this_thread::sleep_for(std::chrono::milliseconds(50));
        js.wait();

        float mean = 0.f;
        for(size_t ii=0; ii<njobs; ++ii)
            mean += means[ii];
        mean /= float(njobs);

        KLOGN("nuclear") << "iter=" << kk << " mean= " << mean << std::endl;
        // experiments.push_back(mean);
    }

    auto dur = clk.get_elapsed_time();
    KLOG("nuclear",1) << "Execution time: " << dur.count() << "us" << std::endl;

/*
    for(size_t kk=0; kk<10; ++kk)
    {
        js.schedule([]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    js.update();
    js.wait();
*/
    return 0;
}