#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "random/random_operation.h"

#include <vector>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("event", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    std::vector<int> numbers = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    for(size_t ii=0; ii<10; ++ii)
    {
        auto it = rng::random_select(numbers.begin(), numbers.end());
        KLOG("nuclear",1) << *it << std::endl;
    }

    return 0;
}
