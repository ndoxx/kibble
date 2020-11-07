#include "logger/logger.h"
#include "logger/logger_sink.h"
#include "logger/logger_thread.h"
#include "argparse/argparse.h"

using namespace kb;

void init_logger()
{
	KLOGGER_START();
	
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_single_threaded(true));
    KLOGGER(set_backtrace_on_error(false));
    KLOGGER(spawn());
    KLOGGER(sync());
}

int main(int argc, char** argv)
{
	init_logger();
    KLOGN("kibble") << "Hello." << std::endl;

	ap::ArgParse parser("Test parser", "0.1");
	parser.add_flag('o', "orange", "Use the best color in the world");
	parser.add_flag('c', "cyan", "Use the second best color in the world");

	parser.parse(argc, argv);
	parser.debug_report();
}