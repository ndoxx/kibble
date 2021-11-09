#include "argparse/argparse.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include <exception>
#include <regex>
#include <string>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("kibble", 3));
    KLOGGER(create_channel("captain", 3));
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
    for(int ii = 0; ii < argc; ++ii)
    {
        KLOG("kibble", 1) << ii << " = " << argv[ii] << std::endl;
    }

    return 0;
}

int p1(int argc, char** argv)
{
    ap::ArgParse parser("nuclear", "0.1");

    const auto& orange = parser.add_flag('o', "orange", "Use the best color in the world");
    const auto& yarr = parser.add_flag('y', "yarr", "Say Yarrrrrr!");
    const auto& age = parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    if(orange())
    {
        KLOG("kibble", 1) << KF_(255, 190, 0);
    }
    KLOG("kibble", 1) << "Age of the captain: " << age() << std::endl;
    if(yarr())
    {
        KLOG("captain", 1) << "Yarrrrrr!" << std::endl;
    }

    return 0;
}

int p2(int argc, char** argv)
{
    ap::ArgParse parser("nuclear", "0.1");

    const auto& orange = parser.add_flag('o', "orange", "Use the best color in the world");
    const auto& A = parser.add_positional<int>("first_number", "the first number to be added");
    const auto& B = parser.add_positional<int>("second_number", "the second number to be added");

    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    if(orange())
    {
        KLOG("kibble", 1) << KF_(255, 190, 0);
    }
    KLOG("kibble", 1) << "The sum of " << A() << " and " << B() << " is " << A() + B() << std::endl;

    return 0;
}

int p3(int argc, char** argv)
{
    ap::ArgParse parser("nuclear", "0.1");
    parser.set_log_output([](const std::string& str) { KLOG("kibble", 1) << str << std::endl; });

    parser.add_flag('A', "param_A", "The parameter A");
    parser.add_flag('B', "param_B", "The parameter B");
    parser.add_flag('C', "param_C", "The parameter C");
    parser.add_flag('D', "param_D", "The parameter D");
    parser.add_flag('E', "param_E", "The parameter E");
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.add_variable<int>('m', "var_m", "The variable m", 10);
    parser.add_variable<int>('n', "var_n", "The variable n", 10);
    parser.add_variable<float>('o', "var_o", "The variable o", 10);
    parser.add_positional<int>("MAGIC", "The magic number");
    parser.set_flags_exclusive({'x', 'y'});
    parser.set_flags_exclusive({'y', 'z'});
    parser.set_variables_exclusive({'m', 'o'});
    parser.set_dependency('D', 'E');

    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    return 0;
}

int p4(int argc, char** argv)
{
    ap::ArgParse parser("nuclear", "0.1");
    parser.set_log_output([](const std::string& str) { KLOG("kibble", 1) << str << std::endl; });

    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    const auto& l = parser.add_list<int>('l', "list_l", "A list of values");
    const auto& mm = parser.add_variable<int>('m', "var_m", "The variable m", 10);
    parser.add_positional<int>("MAGIC", "The magic number");
    parser.set_dependency('y', 'x');

    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    if(mm.is_set)
    {
        KLOG("kibble", 1) << "m: " << mm() << std::endl;
    }

    for(int v : l())
        KLOG("kibble", 1) << v << std::endl;

    KLOGN("kibble") << "done" << std::endl;
    return 0;
}

int p5(int argc, char** argv)
{
    ap::ArgParse parser("nuclear", "0.1");
    const auto& target = parser.add_positional<std::string>("ROM_PATH", "Path to the ROM");
    parser.parse(argc, argv);

    KLOGN("kibble") << "Extracting from: " << std::endl;
    KLOGI << KS_PATH_ << target() << std::endl;

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    init_logger();

    // return p0(argc, argv);
    // return p1(argc, argv);
    // return p2(argc, argv);
    return p3(argc, argv);
    // return p4(argc, argv);
    // return p5(argc, argv);
}