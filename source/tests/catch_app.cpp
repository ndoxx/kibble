/*
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
*/

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>

#include "logger/logger.h"
#include "logger/sink.h"
#include "logger/dispatcher.h"

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("kibble", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char* argv[])
{
    init_logger();

    KLOGN("kibble") << "Running tests." << std::endl;

	int result = Catch::Session().run( argc, argv );

	// global clean-up...

	return result;
}