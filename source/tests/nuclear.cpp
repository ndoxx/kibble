#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "thread/job/config.h"
#include "thread/job/job_graph.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace kb;
using namespace kb::th;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    size_t rr = 0;
    size_t num_thr = 4;

    KBANG();
    for(size_t ii=0; ii<10; ++ii)
    {
        KLOG("nuclear",1) << rr << std::endl;
        rr = (rr + 1) % num_thr;
    }

    // rr = 0;
    // KBANG();
    // for(size_t ii=0; ii<10; ++ii)
    // {
    //     rr += (rr == 0) ? 1 : 0;
    //     rr %= num_thr;
    //     KLOG("nuclear",1) << rr << std::endl;
    //     rr = (rr + 1) % num_thr;
    // }

    rr = 0;
    KBANG();
    for(size_t ii=0; ii<10; ++ii)
    {
        rr = (rr + (rr == 0)) % num_thr;
        KLOG("nuclear",1) << rr << std::endl;
        rr = (rr + 1) % num_thr;
    }

    return 0;
}
