#include "argparse/argparse.h"
#include "test_common.hpp"

using namespace kb;

// Run from build with:
// > ../bin/test/fuzz_tester ../data/fuzz/CORPUS_argparse -jobs=2
// A corpus must exist in the CORPUS_argparse directory, with
// multiple single-line text files containing typical input
// for this program under test.

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ap::ArgParse parser("nuclear", "0.1");
    parser.set_exit_on_special_command(false); // Prevent exiting on --help and --version

    parser.add_flag('A', "param_A", "The parameter A");
    parser.add_flag('B', "param_B", "The parameter B");
    parser.add_flag('C', "param_C", "The parameter C");
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

    auto arguments = tc::tokenize(reinterpret_cast<const char*>(data), size);
    auto argv = tc::make_argv(arguments);
    parser.parse(int(argv.size()) - 1, const_cast<char**>(argv.data()));

    return 0; // Non-zero return values are reserved for future use.
}