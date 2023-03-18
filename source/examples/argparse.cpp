#include "argparse/argparse.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"

#include <exception>
#include <fmt/color.h>
#include <fmt/std.h>
#include <regex>
#include <string>

using namespace kb;
using namespace kb::log;

void show_error_and_die(ap::ArgParse &parser, const Channel &chan)
{
    for (const auto &msg : parser.get_errors())
        klog(chan).warn(msg);

    klog(chan).raw().info(parser.usage());
    exit(0);
}

int p0(int argc, char **argv, const Channel &chan)
{
    for (int ii = 0; ii < argc; ++ii)
    {
        klog(chan).info("{} = {}", ii, argv[ii]);
    }

    return 0;
}

int p1(int argc, char **argv, const Channel &chan)
{
    ap::ArgParse parser("example", "0.1");

    const auto &orange = parser.add_flag('o', "orange", "Use the best color in the world");
    const auto &yarr = parser.add_flag('y', "yarr", "Say Yarrrrrr!");
    const auto &age = parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan);

    if (orange())
    {
        klog(chan).info("Age of the captain: {}", fmt::styled(age(), fmt::fg(fmt::color::orange)));
    }
    else
    {
        klog(chan).info("Age of the captain: {}", age());
    }

    if (yarr())
    {
        klog(chan).uid("Captain").info("Yarrrrrr!");
    }

    return 0;
}

int p2(int argc, char **argv, const Channel &chan)
{
    ap::ArgParse parser("example", "0.1");

    const auto &orange = parser.add_flag('o', "orange", "Use the best color in the world");
    const auto &A = parser.add_positional<int>("first_number", "the first number to be added");
    const auto &B = parser.add_positional<int>("second_number", "the second number to be added");

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan);

    auto fmtstr = fmt::format("The sum of {} and {} is {}", A(), B(), A() + B());
    if (orange())
    {
        klog(chan).info("{}", fmt::styled(fmtstr, fmt::fg(fmt::color::orange)));
    }
    else
    {
        klog(chan).info(fmtstr);
    }

    return 0;
}

int p3(int argc, char **argv, const Channel &chan)
{
    ap::ArgParse parser("example", "0.1");
    parser.set_log_output([&chan](const std::string &str) { klog(chan).uid("ArgParse").info(str); });

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
    if (!success)
        show_error_and_die(parser, chan);

    return 0;
}

int p4(int argc, char **argv, const Channel &chan)
{
    ap::ArgParse parser("example", "0.1");
    parser.set_log_output([&chan](const std::string &str) { klog(chan).uid("ArgParse").info(str); });

    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    const auto &l = parser.add_list<int>('l', "list_l", "A list of values");
    const auto &mm = parser.add_variable<int>('m', "var_m", "The variable m", 10);
    parser.add_positional<int>("MAGIC", "The magic number");
    parser.set_dependency('y', 'x');

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan);

    if (mm.is_set)
    {
        klog(chan).info("m: {}", mm());
    }

    for (int v : l())
        klog(chan).info("v: {}", v);

    klog(chan).info("Done.");
    return 0;
}

int p5(int argc, char **argv, const Channel &chan)
{
    ap::ArgParse parser("example", "0.1");
    const auto &target = parser.add_positional<std::string>("ROM_PATH", "Path to the ROM");
    parser.parse(argc, argv);

    klog(chan).info("Extracting from:\n{}", target());

    return 0;
}

int main(int argc, char **argv)
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan.attach_sink(console_sink);

    // return p0(argc, argv, chan);
    // return p1(argc, argv, chan);
    return p2(argc, argv, chan);
    // return p3(argc, argv, chan);
    // return p4(argc, argv, chan);
    // return p5(argc, argv, chan);
}