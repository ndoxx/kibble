#include "kibble/string/parse.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string_view>

using namespace kb::su;

TEST_CASE("parse_float - Basic integer parsing", "[parse_float]")
{
    float result;

    SECTION("Simple positive integers")
    {
        REQUIRE(parse_float("0", result));
        REQUIRE(result == Catch::Approx(0.0f));

        REQUIRE(parse_float("1", result));
        REQUIRE(result == Catch::Approx(1.0f));

        REQUIRE(parse_float("42", result));
        REQUIRE(result == Catch::Approx(42.0f));

        REQUIRE(parse_float("123", result));
        REQUIRE(result == Catch::Approx(123.0f));
    }

    SECTION("Negative integers")
    {
        REQUIRE(parse_float("-1", result));
        REQUIRE(result == Catch::Approx(-1.0f));

        REQUIRE(parse_float("-42", result));
        REQUIRE(result == Catch::Approx(-42.0f));

        REQUIRE(parse_float("-123", result));
        REQUIRE(result == Catch::Approx(-123.0f));
    }

    SECTION("Explicit positive sign")
    {
        REQUIRE(parse_float("+1", result));
        REQUIRE(result == Catch::Approx(1.0f));

        REQUIRE(parse_float("+42", result));
        REQUIRE(result == Catch::Approx(42.0f));
    }
}

TEST_CASE("parse_float - Decimal parsing", "[parse_float]")
{
    float result;

    SECTION("Simple decimals")
    {
        REQUIRE(parse_float("0.0", result));
        REQUIRE(result == Catch::Approx(0.0f));

        REQUIRE(parse_float("0.5", result));
        REQUIRE(result == Catch::Approx(0.5f));

        REQUIRE(parse_float("1.0", result));
        REQUIRE(result == Catch::Approx(1.0f));

        REQUIRE(parse_float("3.14159", result));
        REQUIRE(result == Catch::Approx(3.14159f));
    }

    SECTION("Negative decimals")
    {
        REQUIRE(parse_float("-0.5", result));
        REQUIRE(result == Catch::Approx(-0.5f));

        REQUIRE(parse_float("-3.14159", result));
        REQUIRE(result == Catch::Approx(-3.14159f));
    }

    SECTION("Decimals without integer part")
    {
        REQUIRE(parse_float(".5", result));
        REQUIRE(result == Catch::Approx(0.5f));

        REQUIRE(parse_float(".125", result));
        REQUIRE(result == Catch::Approx(0.125f));

        REQUIRE(parse_float("-.5", result));
        REQUIRE(result == Catch::Approx(-0.5f));
    }

    SECTION("Decimals without fractional part")
    {
        REQUIRE(parse_float("5.", result));
        REQUIRE(result == Catch::Approx(5.0f));

        REQUIRE(parse_float("42.", result));
        REQUIRE(result == Catch::Approx(42.0f));
    }
}

TEST_CASE("parse_float - Scientific notation", "[parse_float]")
{
    float result;

    SECTION("Positive exponents")
    {
        REQUIRE(parse_float("1e2", result));
        REQUIRE(result == Catch::Approx(100.0f));

        REQUIRE(parse_float("1.5e2", result));
        REQUIRE(result == Catch::Approx(150.0f));

        REQUIRE(parse_float("2.5E3", result));
        REQUIRE(result == Catch::Approx(2500.0f));

        REQUIRE(parse_float("1e+2", result));
        REQUIRE(result == Catch::Approx(100.0f));
    }

    SECTION("Negative exponents")
    {
        REQUIRE(parse_float("1e-2", result));
        REQUIRE(result == Catch::Approx(0.01f));

        REQUIRE(parse_float("5e-1", result));
        REQUIRE(result == Catch::Approx(0.5f));

        REQUIRE(parse_float("2.5E-3", result));
        REQUIRE(result == Catch::Approx(0.0025f));
    }

    SECTION("Negative numbers with scientific notation")
    {
        REQUIRE(parse_float("-1e2", result));
        REQUIRE(result == Catch::Approx(-100.0f));

        REQUIRE(parse_float("-2.5e-3", result));
        REQUIRE(result == Catch::Approx(-0.0025f));
    }
}

TEST_CASE("parse_float - Common values", "[parse_float]")
{
    float result;

    SECTION("Typical values")
    {
        REQUIRE(parse_float("0.0", result));
        REQUIRE(result == Catch::Approx(0.0f));

        REQUIRE(parse_float("0.125", result));
        REQUIRE(result == Catch::Approx(0.125f));

        REQUIRE(parse_float("0.25", result));
        REQUIRE(result == Catch::Approx(0.25f));

        REQUIRE(parse_float("0.5", result));
        REQUIRE(result == Catch::Approx(0.5f));

        REQUIRE(parse_float("0.75", result));
        REQUIRE(result == Catch::Approx(0.75f));

        REQUIRE(parse_float("1.0", result));
        REQUIRE(result == Catch::Approx(1.0f));
    }

    SECTION("Extended range values")
    {
        REQUIRE(parse_float("1.5", result));
        REQUIRE(result == Catch::Approx(1.5f));

        REQUIRE(parse_float("2.0", result));
        REQUIRE(result == Catch::Approx(2.0f));

        REQUIRE(parse_float("0.003921569", result)); // 1/255
        REQUIRE(result == Catch::Approx(0.003921569f).epsilon(1e-6f));
    }
}

TEST_CASE("parse_float - Edge cases and precision", "[parse_float]")
{
    float result;

    SECTION("Very small numbers")
    {
        REQUIRE(parse_float("0.00001", result));
        REQUIRE(result == Catch::Approx(0.00001f));

        REQUIRE(parse_float("1e-10", result));
        REQUIRE(result == Catch::Approx(1e-10f));
    }

    SECTION("Large numbers")
    {
        REQUIRE(parse_float("123456.789", result));
        REQUIRE(result == Catch::Approx(123456.789f));

        REQUIRE(parse_float("1e6", result));
        REQUIRE(result == Catch::Approx(1000000.0f));
    }

    SECTION("High precision decimals")
    {
        REQUIRE(parse_float("0.123456789", result));
        REQUIRE(result == Catch::Approx(0.123456789f).epsilon(1e-6f));

        REQUIRE(parse_float("3.141592653589793", result));
        REQUIRE(result == Catch::Approx(3.141592653589793f).epsilon(1e-6f));
    }
}

TEST_CASE("parse_float - Error cases", "[parse_float]")
{
    float result;

    SECTION("Empty and whitespace")
    {
        REQUIRE_FALSE(parse_float("", result));
        REQUIRE_FALSE(parse_float(" ", result));
        REQUIRE_FALSE(parse_float("  ", result));
        REQUIRE_FALSE(parse_float("\t", result));
        REQUIRE_FALSE(parse_float("\n", result));
    }

    SECTION("Invalid characters")
    {
        REQUIRE_FALSE(parse_float("abc", result));
        REQUIRE_FALSE(parse_float("1.2.3", result));
        REQUIRE_FALSE(parse_float("1a", result));
        REQUIRE_FALSE(parse_float("a1", result));
        REQUIRE_FALSE(parse_float("1.2a", result));
    }

    SECTION("Invalid signs")
    {
        REQUIRE_FALSE(parse_float("++1", result));
        REQUIRE_FALSE(parse_float("--1", result));
        REQUIRE_FALSE(parse_float("+-1", result));
        REQUIRE_FALSE(parse_float("-+1", result));
    }

    SECTION("Invalid scientific notation")
    {
        REQUIRE_FALSE(parse_float("1e", result));
        REQUIRE_FALSE(parse_float("1e+", result));
        REQUIRE_FALSE(parse_float("1e-", result));
        REQUIRE_FALSE(parse_float("1ee2", result));
        REQUIRE_FALSE(parse_float("e2", result));
        REQUIRE_FALSE(parse_float("1e2.5", result));
    }

    SECTION("Only signs or dots")
    {
        REQUIRE_FALSE(parse_float("+", result));
        REQUIRE_FALSE(parse_float("-", result));
        REQUIRE_FALSE(parse_float(".", result));
        REQUIRE_FALSE(parse_float("+.", result));
        REQUIRE_FALSE(parse_float("-.", result));
    }

    SECTION("Trailing/leading invalid characters")
    {
        REQUIRE_FALSE(parse_float("1.0f", result));
        REQUIRE_FALSE(parse_float(" 1.0", result));
        REQUIRE_FALSE(parse_float("1.0 ", result));
        REQUIRE_FALSE(parse_float("x1.0", result));
    }
}

TEST_CASE("parse_float - Boundary values", "[parse_float]")
{
    float result;

    SECTION("Zero variations")
    {
        REQUIRE(parse_float("0", result));
        REQUIRE(result == Catch::Approx(0.0f));

        REQUIRE(parse_float("-0", result));
        REQUIRE(result == Catch::Approx(0.0f));

        REQUIRE(parse_float("0.0", result));
        REQUIRE(result == Catch::Approx(0.0f));

        REQUIRE(parse_float("0e10", result));
        REQUIRE(result == Catch::Approx(0.0f));
    }

    SECTION("Single digit tests")
    {
        for (int i = 0; i <= 9; ++i)
        {
            std::string s = std::to_string(i);
            REQUIRE(parse_float(s, result));
            REQUIRE(result == Catch::Approx(static_cast<float>(i)));
        }
    }
}

TEST_CASE("parse_float - String view specifics", "[parse_float]")
{
    float result;

    SECTION("Substring of larger string")
    {
        std::string larger = "prefix 3.14159 suffix";
        std::string_view sv = std::string_view(larger).substr(7, 7); // "3.14159"

        REQUIRE(parse_float(sv, result));
        REQUIRE(result == Catch::Approx(3.14159f));
    }

    SECTION("String view from const char*")
    {
        const char* cstr = "2.71828";
        std::string_view sv(cstr);

        REQUIRE(parse_float(sv, result));
        REQUIRE(result == Catch::Approx(2.71828f));
    }
}