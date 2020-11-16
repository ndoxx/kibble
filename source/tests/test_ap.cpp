#include "argparse/argparse.h"
#include "test_common.hpp"
#include <catch2/catch_all.hpp>

using namespace kb;

bool Parse(ap::ArgParse& parser, const std::string& input)
{
    auto arguments = tc::tokenize(input);
    auto argv = tc::make_argv(arguments);
    return parser.parse(int(argv.size()) - 1, const_cast<char**>(argv.data()));
}

class FlagFixture
{
public:
    FlagFixture()
        : parser("program", "0.1"), orange(parser.add_flag('o', "orange", "Use the best color in the world")),
          cyan(parser.add_flag('c', "cyan", "Use the second best color in the world"))
    {}

protected:
    ap::ArgParse parser;
    const ap::Flag& orange;
    const ap::Flag& cyan;
};

TEST_CASE_METHOD(FlagFixture, "Flag argument parsing default", "[flag]")
{
    bool success = Parse(parser, "program");

    REQUIRE(success);
    REQUIRE(!orange());
    REQUIRE(!cyan());
}

TEST_CASE_METHOD(FlagFixture, "Flag argument, short name", "[flag]")
{
    bool success = Parse(parser, "program -o");

    REQUIRE(success);
    REQUIRE(orange());
    REQUIRE(!cyan());
}

TEST_CASE_METHOD(FlagFixture, "Flag argument, full name", "[flag]")
{
    bool success = Parse(parser, "program --orange");

    REQUIRE(success);
    REQUIRE(orange());
    REQUIRE(!cyan());
}

TEST_CASE_METHOD(FlagFixture, "Multiple flag arguments, short name only, no concat", "[flag]")
{
    bool success = Parse(parser, "program -o -c");

    REQUIRE(success);
    REQUIRE(orange());
    REQUIRE(cyan());
}

TEST_CASE_METHOD(FlagFixture, "Multiple flag arguments, short name only, concat", "[flag]")
{
    bool success = Parse(parser, "program -oc");

    REQUIRE(success);
    REQUIRE(orange());
    REQUIRE(cyan());
}

TEST_CASE_METHOD(FlagFixture, "Multiple flag arguments, mixed name length", "[flag]")
{
    bool success = Parse(parser, "program -o --cyan");

    REQUIRE(success);
    REQUIRE(orange());
    REQUIRE(cyan());
}

TEST_CASE_METHOD(FlagFixture, "Flag, bad syntax", "[flag]")
{
    bool success = Parse(parser, "program -cyan");

    REQUIRE(!success);
}

TEST_CASE_METHOD(FlagFixture, "Unknown flag", "[flag]")
{
    bool success = Parse(parser, "program --green");

    REQUIRE(!success);
    REQUIRE(!orange());
    REQUIRE(!cyan());
}

class VarFixture
{
public:
    VarFixture() : parser("program", "0.1") {}

protected:
    ap::ArgParse parser;
};

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, default", "[var]")
{
    const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program");

    REQUIRE(success);
    REQUIRE(!var.is_set);
    REQUIRE(var() == 42);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, short name", "[var]")
{
    const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program -a 56");

    REQUIRE(success);
    REQUIRE(var.is_set);
    REQUIRE(var() == 56);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, short name, missing value", "[var]")
{
    parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program -a");

    REQUIRE(!success);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, full name", "[var]")
{
    const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program --age 56");

    REQUIRE(success);
    REQUIRE(var.is_set);
    REQUIRE(var() == 56);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, full name, missing value", "[var]")
{
    parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program --age");

    REQUIRE(!success);
}

TEST_CASE_METHOD(VarFixture, "Unknown variable, short name", "[var]")
{
    parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program -p 56");

    REQUIRE(!success);
}

TEST_CASE_METHOD(VarFixture, "Unknown variable, full name", "[var]")
{
    parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program --page 56");

    REQUIRE(!success);
}

TEST_CASE_METHOD(VarFixture, "Variable argument, cast failure", "[var]")
{
    parser.add_variable<int>('a', "age", "Age of the captain", 42);

    bool success = Parse(parser, "program --age plop");

    REQUIRE(!success);
}

TEST_CASE_METHOD(VarFixture, "Two variable <int> arguments, full name used", "[var]")
{
    const auto& age = parser.add_variable<int>('a', "age", "Age of the captain", 42);
    const auto& height = parser.add_variable<int>('x', "height", "Height of the captain", 180);

    bool success = Parse(parser, "program --age 56 --height 185");

    REQUIRE(success);
    REQUIRE(age.is_set);
    REQUIRE(age() == 56);
    REQUIRE(height.is_set);
    REQUIRE(height() == 185);
}

TEST_CASE_METHOD(VarFixture, "Two variable <int> arguments, short name used", "[var]")
{
    const auto& age = parser.add_variable<int>('a', "age", "Age of the captain", 42);
    const auto& height = parser.add_variable<int>('x', "height", "Height of the captain", 180);

    bool success = Parse(parser, "program -a 56 -x 185");

    REQUIRE(success);
    REQUIRE(age.is_set);
    REQUIRE(age() == 56);
    REQUIRE(height.is_set);
    REQUIRE(height() == 185);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, hexadecimal repr", "[var]")
{
    const auto& var = parser.add_variable<int>('o', "offset", "Offset of the captain", 0x10);

    bool success = Parse(parser, "program -o 0x22");

    REQUIRE(success);
    REQUIRE(var.is_set);
    REQUIRE(var() == 0x22);
}

TEST_CASE_METHOD(VarFixture, "Variable <long> argument, hexadecimal repr", "[var]")
{
    const auto& var = parser.add_variable<long>('o', "offset", "Offset of the captain", 0x10);

    bool success = Parse(parser, "program -o 0xff45289c4565");

    REQUIRE(success);
    REQUIRE(var.is_set);
    REQUIRE(var() == 0xff45289c4565);
}

TEST_CASE_METHOD(VarFixture, "Variable <float> argument, valid input", "[var]")
{
    const auto& x = parser.add_variable<float>('x', "var_x", "The x variable", 0.f);
    const auto& y = parser.add_variable<float>('y', "var_y", "The y variable", 0.f);
    const auto& z = parser.add_variable<float>('z', "var_z", "The z variable", 0.f);

    bool success = Parse(parser, "program -x 1 -y 1.2345 -z 1.2345e-1");

    REQUIRE(success);
    REQUIRE(x.is_set);
    REQUIRE(x() == 1.f);
    REQUIRE(y.is_set);
    REQUIRE(y() == 1.2345f);
    REQUIRE(z.is_set);
    REQUIRE(z() == 1.2345e-1f);
}

TEST_CASE_METHOD(VarFixture, "Variable <double> argument, valid input", "[var]")
{
    const auto& x = parser.add_variable<double>('x', "var_x", "The x variable", 0.0);
    const auto& y = parser.add_variable<double>('y', "var_y", "The y variable", 0.0);
    const auto& z = parser.add_variable<double>('z', "var_z", "The z variable", 0.0);

    bool success = Parse(parser, "program -x 1 -y 1.2345 -z 1.2345e-100");

    REQUIRE(success);
    REQUIRE(x.is_set);
    REQUIRE(x() == 1.0);
    REQUIRE(y.is_set);
    REQUIRE(y() == 1.2345);
    REQUIRE(z.is_set);
    REQUIRE(z() == 1.2345e-100);
}

// TODO: test with spaces
TEST_CASE_METHOD(VarFixture, "Variable <string> argument", "[var]")
{
    const auto& s = parser.add_variable<std::string>('s', "var_s", "The s variable", "");

    bool success = Parse(parser, "program -s plip_plop");

    REQUIRE(success);
    REQUIRE(s.is_set);
    REQUIRE(!s().compare("plip_plop"));
}

class PosFixture
{
public:
    PosFixture() : parser("program", "0.1") {}

protected:
    ap::ArgParse parser;
};

TEST_CASE_METHOD(PosFixture, "Single positional int argument", "[pos]")
{
    const auto& A = parser.add_positional<int>("A", "First number");

    bool success = Parse(parser, "program 42");

    REQUIRE(success);
    REQUIRE(A.is_set);
    REQUIRE(A() == 42);
}

TEST_CASE_METHOD(PosFixture, "Single positional string argument", "[pos]")
{
    const auto& A = parser.add_positional<std::string>("A", "First number");

    bool success = Parse(parser, "program plop");

    REQUIRE(success);
    REQUIRE(A.is_set);
    REQUIRE(!A().compare("plop"));
}

TEST_CASE_METHOD(PosFixture, "Three positional arguments", "[pos]")
{
    const auto& A = parser.add_positional<int>("A", "First number");
    const auto& B = parser.add_positional<int>("B", "Second number");
    const auto& C = parser.add_positional<int>("C", "Third number");

    bool success = Parse(parser, "program 42 43 44");

    REQUIRE(success);
    REQUIRE(A.is_set);
    REQUIRE(A() == 42);
    REQUIRE(B.is_set);
    REQUIRE(B() == 43);
    REQUIRE(C.is_set);
    REQUIRE(C() == 44);
}

TEST_CASE_METHOD(PosFixture, "Two positional arguments, missing one", "[pos]")
{
    parser.add_positional<int>("A", "First number");
    parser.add_positional<int>("B", "Second number");

    bool success = Parse(parser, "program 42");

    REQUIRE(!success);
}

TEST_CASE_METHOD(PosFixture, "One positional argument needed, supernumerary one", "[pos]")
{
    parser.add_positional<int>("A", "First number");

    bool success = Parse(parser, "program 42 43");

    REQUIRE(!success);
}

class ExFlagFixture
{
public:
    ExFlagFixture() : parser("program", "0.1") {}

protected:
    ap::ArgParse parser;
};

TEST_CASE_METHOD(ExFlagFixture, "exf: Two exclusive flags, constraint obeyed", "[exf]")
{
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.set_flags_exclusive({'x', 'y'});

    bool success = Parse(parser, "program -x -z");

    REQUIRE(success);
}

TEST_CASE_METHOD(ExFlagFixture, "exf: Two exclusive flags, constraint violated", "[exf]")
{
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.set_flags_exclusive({'x', 'y'});

    bool success = Parse(parser, "program -x -y -z");

    REQUIRE(!success);
}

TEST_CASE_METHOD(ExFlagFixture, "exf: Two exclusive sets, constraint obeyed", "[exf]")
{
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.set_flags_exclusive({'x', 'y'});
    parser.set_flags_exclusive({'y', 'z'});

    bool success = Parse(parser, "program -x -z");

    REQUIRE(success);
}

TEST_CASE_METHOD(ExFlagFixture, "exf: Two exclusive sets, constraint violated", "[exf]")
{
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.set_flags_exclusive({'x', 'y'});
    parser.set_flags_exclusive({'y', 'z'});

    bool success = Parse(parser, "program -y -z");

    REQUIRE(!success);
}

class ExVarFixture
{
public:
    ExVarFixture() : parser("program", "0.1") {}

protected:
    ap::ArgParse parser;
};

TEST_CASE_METHOD(ExVarFixture, "exv: Two exclusive variables, constraint obeyed", "[exv]")
{
    parser.add_variable<int>('x', "var_x", "The x variable", 0);
    parser.add_variable<int>('y', "var_y", "The y variable", 0);
    parser.add_variable<int>('z', "var_z", "The z variable", 0);
    parser.set_variables_exclusive({'x', 'y'});

    bool success = Parse(parser, "program -x 10 -z 10");

    REQUIRE(success);
}

TEST_CASE_METHOD(ExVarFixture, "exv: Two exclusive variables, constraint violated", "[exv]")
{
    parser.add_variable<int>('x', "var_x", "The x variable", 0);
    parser.add_variable<int>('y', "var_y", "The y variable", 0);
    parser.add_variable<int>('z', "var_z", "The z variable", 0);
    parser.set_variables_exclusive({'x', 'y'});

    bool success = Parse(parser, "program -x 10 -y 10");

    REQUIRE(!success);
}

TEST_CASE_METHOD(ExVarFixture, "exv: Two exclusive sets, constraint obeyed", "[exv]")
{
    parser.add_variable<int>('x', "var_x", "The x variable", 0);
    parser.add_variable<int>('y', "var_y", "The y variable", 0);
    parser.add_variable<int>('z', "var_z", "The z variable", 0);
    parser.set_variables_exclusive({'x', 'y'});
    parser.set_variables_exclusive({'y', 'z'});

    bool success = Parse(parser, "program -x 10 -z 10");

    REQUIRE(success);
}

TEST_CASE_METHOD(ExVarFixture, "exv: Two exclusive sets, constraint violated", "[exv]")
{
    parser.add_variable<int>('x', "var_x", "The x variable", 0);
    parser.add_variable<int>('y', "var_y", "The y variable", 0);
    parser.add_variable<int>('z', "var_z", "The z variable", 0);
    parser.set_variables_exclusive({'x', 'y'});
    parser.set_variables_exclusive({'y', 'z'});

    bool success = Parse(parser, "program -y 10 -z 10");

    REQUIRE(!success);
}

class DepsFixture
{
public:
    DepsFixture() : parser("program", "0.1") {}

protected:
    ap::ArgParse parser;
};

TEST_CASE_METHOD(DepsFixture, "Flag dependency satisfied", "[dep]")
{
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.set_dependency('y', 'x');

    bool success = Parse(parser, "program -xyz");

    REQUIRE(success);
}

TEST_CASE_METHOD(DepsFixture, "Flag dependency not satisfied", "[dep]")
{
    parser.add_flag('x', "param_x", "The parameter x");
    parser.add_flag('y', "param_y", "The parameter y");
    parser.add_flag('z', "param_z", "The parameter z");
    parser.set_dependency('y', 'x');

    bool success = Parse(parser, "program -yz");

    REQUIRE(!success);
}

class ListsFixture
{
public:
    ListsFixture() : parser("program", "0.1") {}

protected:
    ap::ArgParse parser;
};

TEST_CASE_METHOD(ListsFixture, "Valid list of ints", "[lis]")
{
    const auto& l = parser.add_list<int>('l', "list_l", "A list of values");

    bool success = Parse(parser, "program -l 1,2,-3,4");

    std::vector<int> ans = {1,2,-3,4};
    REQUIRE(success);
    REQUIRE(l() == ans);
}

TEST_CASE_METHOD(ListsFixture, "Invalid list of ints", "[lis]")
{
    parser.add_list<int>('l', "list_l", "A list of values");

    bool success = Parse(parser, "program -l 1,b,a,4");

    REQUIRE(!success);
}