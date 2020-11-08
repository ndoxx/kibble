#include "logger/logger.h"
#include "logger/logger_sink.h"
#include "logger/logger_thread.h"
#include "argparse/argparse.h"

#include <string>
#include <exception>

using namespace kb;

void init_logger()
{
	KLOGGER_START();
	
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("captain", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_single_threaded(true));
    KLOGGER(set_backtrace_on_error(false));
    KLOGGER(spawn());
    KLOGGER(sync());
}

int p1(int argc, char** argv)
{
	ap::ArgParse parser("nuclear", "0.1");

	parser.add_flag('o', "orange", "Use the best color in the world");
	parser.add_flag('y', "yarr", "Say Yarrrrrr!");
	const auto& age = parser.add_variable<int>('a', "age", "Age of the captain", 42);
	bool success = parser.parse(argc, argv);

	parser.usage();
	if(!success)
	{
		return 0;
	}

	if(parser.is_set('o'))
	{
		KLOG("kibble",1) << WCC(255,190,0);
	}
	KLOG("kibble",1) << "Age of the captain: " << age.value << std::endl;
	if(parser.is_set('y'))
	{
		KLOG("captain",1) << "Yarrrrrr!" << std::endl;
	}

	return 0;
}

int p2(int argc, char** argv)
{
	ap::ArgParse parser("nuclear", "0.1");

	parser.add_flag('o', "orange", "Use the best color in the world");
	const auto& A = parser.add_positional<int>("first_number", "the first number to be added");
	const auto& B = parser.add_positional<int>("second_number", "the second number to be added");

	bool success = parser.parse(argc, argv);
	if(!success)
	{
		parser.usage();
		return 0;
	}

	if(parser.is_set('o'))
	{
		KLOG("kibble",1) << WCC(255,190,0);
	}
	KLOG("kibble",1) << "The sum of " << A.value << " and " << B.value << " is " << A.value+B.value << std::endl;

	return 0;
}

int p3(int argc, char** argv)
{
	ap::ArgParse parser("nuclear", "0.1");

	parser.add_flag('A', "param_A", "The parameter A");
	parser.add_flag('B', "param_B", "The parameter B");
	parser.add_flag('C', "param_C", "The parameter C");
	parser.add_flag('x', "param_x", "The parameter x");
	parser.add_flag('y', "param_y", "The parameter y");
	parser.add_flag('z', "param_z", "The parameter z");
	parser.set_flags_exclusive({'x', 'y'});
	parser.set_flags_exclusive({'y', 'z'});
	parser.add_variable<int>('m', "var_m", "The variable m", 10);
	parser.add_variable<int>('n', "var_n", "The variable n", 10);
	parser.add_positional<int>("magic", "The magic number");

	parser.usage();

	bool success = parser.parse(argc, argv);

	if(success)
	{
		KLOGG("kibble") << "Success!" << std::endl;
	}

	return 0;
}

int main(int argc, char** argv)
{
	init_logger();

	ap::ArgParse parser("nuclear", "0.1");
	
	// return p1(argc, argv);
	// return p2(argc, argv);
	return p3(argc, argv);
}