#include <catch2/catch_all.hpp>
#include "logger/logger.h"
#include "argparse/argparse.h"

using namespace kb;

class FlagFixture
{
public:
	FlagFixture():
	parser("program", "0.1")
	{
		parser.add_flag('o', "orange", "Use the best color in the world");
		parser.add_flag('c', "cyan", "Use the second best color in the world");
	}

protected:
	ap::ArgParse parser;
};

TEST_CASE_METHOD(FlagFixture, "Flag argument parsing default", "[flag]")
{
	int argc = 1;
	char a1[] = "test_flag";
	char* argv[] = {a1};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(!parser.is_set('o'));
	REQUIRE(!parser.is_set('c'));
}

TEST_CASE_METHOD(FlagFixture, "Flag argument, short name", "[flag]")
{
	int argc = 2;
	char a1[] = "test_flag";
	char a2[] = "-o";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(parser.is_set('o'));
	REQUIRE(!parser.is_set('c'));
}

TEST_CASE_METHOD(FlagFixture, "Flag argument, full name", "[flag]")
{
	int argc = 2;
	char a1[] = "test_flag";
	char a2[] = "--orange";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(parser.is_set('o'));
	REQUIRE(!parser.is_set('c'));
}

TEST_CASE_METHOD(FlagFixture, "Multiple flag arguments, short name only, no concat", "[flag]")
{
	int argc = 3;
	char a1[] = "test_flag";
	char a2[] = "-o";
	char a3[] = "-c";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(parser.is_set('o'));
	REQUIRE(parser.is_set('c'));
}

TEST_CASE_METHOD(FlagFixture, "Multiple flag arguments, short name only, concat", "[flag]")
{
	int argc = 2;
	char a1[] = "test_flag";
	char a2[] = "-oc";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(parser.is_set('o'));
	REQUIRE(parser.is_set('c'));
}

TEST_CASE_METHOD(FlagFixture, "Multiple flag arguments, mixed name length", "[flag]")
{
	int argc = 3;
	char a1[] = "test_flag";
	char a2[] = "-o";
	char a3[] = "--cyan";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(parser.is_set('o'));
	REQUIRE(parser.is_set('c'));
}

TEST_CASE_METHOD(FlagFixture, "Flag, bad syntax", "[flag]")
{
	int argc = 2;
	char a1[] = "test_flag";
	char a2[] = "-cyan";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
}

TEST_CASE_METHOD(FlagFixture, "Unknown flag", "[flag]")
{
	int argc = 2;
	char a1[] = "test_flag";
	char a2[] = "--green";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
	REQUIRE(!parser.is_set('o'));
	REQUIRE(!parser.is_set('c'));
}




class VarFixture
{
public:
	VarFixture():
	parser("program", "0.1")
	{
		
	}

protected:
	ap::ArgParse parser;
};

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, default", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 1;
	char a1[] = "test_var";
	char* argv[] = {a1};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(!var.is_set);
	REQUIRE(var.value == 42);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, short name", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 3;
	char a1[] = "test_var";
	char a2[] = "-a";
	char a3[] = "56";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(var.is_set);
	REQUIRE(var.value == 56);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, short name, missing value", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 2;
	char a1[] = "test_var";
	char a2[] = "-a";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
	REQUIRE(!var.is_set);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, full name", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 3;
	char a1[] = "test_var";
	char a2[] = "--age";
	char a3[] = "56";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(var.is_set);
	REQUIRE(var.value == 56);
}

TEST_CASE_METHOD(VarFixture, "Variable <int> argument, full name, missing value", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 2;
	char a1[] = "test_var";
	char a2[] = "--age";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
	REQUIRE(!var.is_set);
}

TEST_CASE_METHOD(VarFixture, "Unknown variable, short name", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 3;
	char a1[] = "test_var";
	char a2[] = "-p";
	char a3[] = "56";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
	REQUIRE(!var.is_set);
}

TEST_CASE_METHOD(VarFixture, "Unknown variable, full name", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 3;
	char a1[] = "test_var";
	char a2[] = "--page";
	char a3[] = "56";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
	REQUIRE(!var.is_set);
}

TEST_CASE_METHOD(VarFixture, "Variable argument, cast failure", "[var]")
{
	const auto& var = parser.add_variable<int>('a', "age", "Age of the captain", 42);

	int argc = 3;
	char a1[] = "test_var";
	char a2[] = "--age";
	char a3[] = "plop";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
	REQUIRE(!var.is_set);
}

TEST_CASE_METHOD(VarFixture, "Two variable <int> arguments, full name used", "[var]")
{
	const auto& age = parser.add_variable<int>('a', "age", "Age of the captain", 42);
	const auto& height = parser.add_variable<int>('h', "height", "Height of the captain", 180);

	int argc = 5;
	char a1[] = "test_var";
	char a2[] = "--age";
	char a3[] = "56";
	char a4[] = "--height";
	char a5[] = "185";
	char* argv[] = {a1, a2, a3, a4, a5};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(age.is_set);
	REQUIRE(age.value == 56);
	REQUIRE(height.is_set);
	REQUIRE(height.value == 185);
}

TEST_CASE_METHOD(VarFixture, "Two variable <int> arguments, short name used", "[var]")
{
	const auto& age = parser.add_variable<int>('a', "age", "Age of the captain", 42);
	const auto& height = parser.add_variable<int>('h', "height", "Height of the captain", 180);

	int argc = 5;
	char a1[] = "test_var";
	char a2[] = "-a";
	char a3[] = "56";
	char a4[] = "-h";
	char a5[] = "185";
	char* argv[] = {a1, a2, a3, a4, a5};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(age.is_set);
	REQUIRE(age.value == 56);
	REQUIRE(height.is_set);
	REQUIRE(height.value == 185);
}



class PosFixture
{
public:
	PosFixture():
	parser("program", "0.1")
	{
		
	}

protected:
	ap::ArgParse parser;
};

TEST_CASE_METHOD(PosFixture, "Single positional argument", "[pos]")
{
	const auto& A = parser.add_positional<int>("A", "First number");
	
	int argc = 2;
	char a1[] = "test_pos";
	char a2[] = "42";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(A.is_set);
	REQUIRE(A.value == 42);
}

TEST_CASE_METHOD(PosFixture, "Three positional arguments", "[pos]")
{
	const auto& A = parser.add_positional<int>("A", "First number");
	const auto& B = parser.add_positional<int>("B", "Second number");
	const auto& C = parser.add_positional<int>("C", "Third number");
	
	int argc = 4;
	char a1[] = "test_pos";
	char a2[] = "42";
	char a3[] = "43";
	char a4[] = "44";
	char* argv[] = {a1, a2, a3, a4};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
	REQUIRE(A.is_set);
	REQUIRE(A.value == 42);
	REQUIRE(B.is_set);
	REQUIRE(B.value == 43);
	REQUIRE(C.is_set);
	REQUIRE(C.value == 44);
}

TEST_CASE_METHOD(PosFixture, "Two positional arguments, missing one", "[pos]")
{
	parser.add_positional<int>("A", "First number");
	parser.add_positional<int>("B", "Second number");
	
	int argc = 2;
	char a1[] = "test_pos";
	char a2[] = "42";
	char* argv[] = {a1, a2};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
}

TEST_CASE_METHOD(PosFixture, "One positional argument needed, supernumerary one", "[pos]")
{
	parser.add_positional<int>("A", "First number");
	
	int argc = 3;
	char a1[] = "test_pos";
	char a2[] = "42";
	char a3[] = "43";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
}



class ExFlagFixture
{
public:
	ExFlagFixture():
	parser("program", "0.1")
	{

	}

protected:
	ap::ArgParse parser;
};

TEST_CASE_METHOD(ExFlagFixture, "Two exclusive flags, constraint obeyed", "[exf]")
{
	parser.add_flag('x', "param_x", "The parameter x");
	parser.add_flag('y', "param_y", "The parameter y");
	parser.add_flag('z', "param_z", "The parameter z");
	parser.set_flags_exclusive({'x', 'y'});
	
	int argc = 3;
	char a1[] = "test_exf";
	char a2[] = "-x";
	char a3[] = "-z";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
}

TEST_CASE_METHOD(ExFlagFixture, "Two exclusive flags, constraint violated", "[exf]")
{
	parser.add_flag('x', "param_x", "The parameter x");
	parser.add_flag('y', "param_y", "The parameter y");
	parser.add_flag('z', "param_z", "The parameter z");
	parser.set_flags_exclusive({'x', 'y'});
	
	int argc = 4;
	char a1[] = "test_exf";
	char a2[] = "-x";
	char a3[] = "-y";
	char a4[] = "-z";
	char* argv[] = {a1, a2, a3, a4};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
}

TEST_CASE_METHOD(ExFlagFixture, "Two exclusive sets, constraint obeyed", "[exf]")
{
	parser.add_flag('x', "param_x", "The parameter x");
	parser.add_flag('y', "param_y", "The parameter y");
	parser.add_flag('z', "param_z", "The parameter z");
	parser.set_flags_exclusive({'x', 'y'});
	parser.set_flags_exclusive({'y', 'z'});
	
	int argc = 3;
	char a1[] = "test_exf";
	char a2[] = "-x";
	char a3[] = "-z";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(success);
}

TEST_CASE_METHOD(ExFlagFixture, "Two exclusive sets, constraint violated", "[exf]")
{
	parser.add_flag('x', "param_x", "The parameter x");
	parser.add_flag('y', "param_y", "The parameter y");
	parser.add_flag('z', "param_z", "The parameter z");
	parser.set_flags_exclusive({'x', 'y'});
	parser.set_flags_exclusive({'y', 'z'});
	
	int argc = 3;
	char a1[] = "test_exf";
	char a2[] = "-y";
	char a3[] = "-z";
	char* argv[] = {a1, a2, a3};

	bool success = parser.parse(argc, argv);

	REQUIRE(!success);
}