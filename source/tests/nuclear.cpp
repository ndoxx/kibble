#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include <vector>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    KLOG("nuclear",1) << "plop" << std::endl;

    return 0;
}
