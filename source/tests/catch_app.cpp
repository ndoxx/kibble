#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
using namespace kb;

void init_logger()
{
    KLOGGER_START();
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("ios", 3));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char* argv[])
{
    init_logger();
    int result = Catch::Session().run(argc, argv);

    // global clean-up...

    return result;
}