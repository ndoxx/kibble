#include "argparse/argparse.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "logger/dispatcher.h"
#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "thread/job2/task_system.h"
#include "time/clock.h"

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

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("thread", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void show_error_and_die(ap::ArgParse& parser)
{
    for(const auto& msg : parser.get_errors())
        KLOGW("kibble") << msg << std::endl;

    KLOG("kibble", 1) << parser.usage() << std::endl;
    exit(0);
}

int p0(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    return 0;
}

int main(int argc, char** argv)
{
    init_logger();

    return p0(argc, argv);
}