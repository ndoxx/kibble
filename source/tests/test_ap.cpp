#include <catch2/catch_all.hpp>
#include "logger/logger.h"
#include "argparse/argparse.h"

using namespace kb;

class FlagFixture
{
public:
	FlagFixture():
	parser("Test parser", "0.1")
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
	REQUIRE(!parser.is_set('o'));
	REQUIRE(!parser.is_set('c'));
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
	parser("Test parser", "0.1")
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