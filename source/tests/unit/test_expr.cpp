// SPDX-License-Identifier: Zlib
/*
 * Catch2 unit tests for the tinyexpr C++ port.
 *
 * Organized by area:
 *   - compile()/interpret() basic contract (success/error shape)
 *   - literals, arithmetic, precedence, associativity, unary sign
 *   - variables (Variable::bind), including re-evaluation after mutation
 *   - built-in functions: 0-arg, 1-arg (tight-binding), 2-arg (parenthesized)
 *   - user-registered functions via Variable::function<Fn> and Variable::closure
 *   - the comma/list operator
 *   - lexical edge cases: whitespace, hex literals, malformed numbers
 *   - every ErrorKind, with position where it's meaningful
 *   - Expression move semantics and to_string()
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kibble/math/expr.h"

#include <cmath>
#include <numbers>
#include <string>

using namespace kb::expr;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
constexpr double k_epsilon = 1e-9;

double eval_of(std::string_view expr, std::span<const Variable> vars = {})
{
    auto result = interpret(expr, vars);
    REQUIRE(result.has_value());
    return *result;
}

ParseError error_of(std::string_view expr, std::span<const Variable> vars = {})
{
    auto result = interpret(expr, vars);
    REQUIRE_FALSE(result.has_value());
    return result.error();
}
} // namespace

// ---------------------------------------------------------------------
// compile() / interpret() basic contract
// ---------------------------------------------------------------------

TEST_CASE("compile succeeds on a trivial expression", "[compile]")
{
    auto compiled = compile("1");
    REQUIRE(compiled.has_value());
    CHECK_THAT(compiled->eval(), WithinAbs(1.0, k_epsilon));
}

TEST_CASE("compile fails on an empty expression", "[compile][errors]")
{
    auto compiled = compile("");
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().kind == ErrorKind::EmptyExpression);
    CHECK(compiled.error().position == 0);
}

TEST_CASE("interpret is equivalent to compile-then-eval", "[interpret]")
{
    CHECK_THAT(eval_of("2 + 3 * 4"), WithinAbs(14.0, k_epsilon));
}

TEST_CASE("interpret propagates parse errors", "[interpret][errors]")
{
    auto result = interpret("(1 + 2");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnbalancedParentheses);
}

TEST_CASE("ParseError carries a human-readable message", "[errors]")
{
    auto compiled = compile("");
    REQUIRE_FALSE(compiled.has_value());
    CHECK_FALSE(compiled.error().message.empty());
}

// ---------------------------------------------------------------------
// Literals
// ---------------------------------------------------------------------

TEST_CASE("integer and decimal literals evaluate to themselves", "[literals]")
{
    CHECK_THAT(eval_of("0"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("42"), WithinAbs(42.0, k_epsilon));
    CHECK_THAT(eval_of("3.14159"), WithinAbs(3.14159, k_epsilon));
    CHECK_THAT(eval_of(".5"), WithinAbs(0.5, k_epsilon));
}

TEST_CASE("scientific notation literals are parsed", "[literals]")
{
    CHECK_THAT(eval_of("1e2"), WithinAbs(100.0, k_epsilon));
    CHECK_THAT(eval_of("1.5e-2"), WithinAbs(0.015, k_epsilon));
}

TEST_CASE("hexadecimal literals are parsed as integers", "[literals][lexer]")
{
    CHECK_THAT(eval_of("0x10"), WithinAbs(16.0, k_epsilon));
    CHECK_THAT(eval_of("0xFF"), WithinAbs(255.0, k_epsilon));
    CHECK_THAT(eval_of("0Xa"), WithinAbs(10.0, k_epsilon));
}

TEST_CASE("whitespace between tokens is ignored", "[lexer]")
{
    CHECK_THAT(eval_of("  1   +   2  "), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("1\t+\n2\r"), WithinAbs(3.0, k_epsilon));
}

// ---------------------------------------------------------------------
// Arithmetic, precedence, associativity
// ---------------------------------------------------------------------

TEST_CASE("basic binary operators", "[arithmetic]")
{
    CHECK_THAT(eval_of("2 + 3"), WithinAbs(5.0, k_epsilon));
    CHECK_THAT(eval_of("5 - 2"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("4 * 5"), WithinAbs(20.0, k_epsilon));
    CHECK_THAT(eval_of("10 / 4"), WithinAbs(2.5, k_epsilon));
    CHECK_THAT(eval_of("10 % 3"), WithinAbs(1.0, k_epsilon));
    CHECK_THAT(eval_of("2 ^ 10"), WithinAbs(1024.0, k_epsilon));
}

TEST_CASE("multiplication and division bind tighter than addition and subtraction", "[arithmetic][precedence]")
{
    CHECK_THAT(eval_of("2 + 3 * 4"), WithinAbs(14.0, k_epsilon));
    CHECK_THAT(eval_of("2 * 3 + 4"), WithinAbs(10.0, k_epsilon));
    CHECK_THAT(eval_of("20 - 4 / 2"), WithinAbs(18.0, k_epsilon));
}

TEST_CASE("exponentiation binds tighter than multiplication", "[arithmetic][precedence]")
{
    CHECK_THAT(eval_of("2 * 3 ^ 2"), WithinAbs(18.0, k_epsilon));
}

TEST_CASE("exponentiation associates right to left in this port", "[arithmetic][precedence]")
{
    // 2^(3^2) = 2^9 = 512, as opposed to the left-associative (2^3)^2 = 64.
    CHECK_THAT(eval_of("2 ^ 3 ^ 2"), WithinAbs(512.0, k_epsilon));
    CHECK_THAT(eval_of("(2 ^ 3) ^ 2"), WithinAbs(64.0, k_epsilon));
}

TEST_CASE("addition and subtraction are left associative", "[arithmetic][precedence]")
{
    CHECK_THAT(eval_of("10 - 3 - 2"), WithinAbs(5.0, k_epsilon));
}

TEST_CASE("division is left associative", "[arithmetic][precedence]")
{
    CHECK_THAT(eval_of("100 / 5 / 2"), WithinAbs(10.0, k_epsilon));
}

TEST_CASE("parentheses override default precedence", "[arithmetic][precedence]")
{
    CHECK_THAT(eval_of("(2 + 3) * 4"), WithinAbs(20.0, k_epsilon));
    CHECK_THAT(eval_of("2 * (3 + 4)"), WithinAbs(14.0, k_epsilon));
}

TEST_CASE("nested parentheses", "[arithmetic][precedence]")
{
    CHECK_THAT(eval_of("((1 + 2) * (3 + 4))"), WithinAbs(21.0, k_epsilon));
}

TEST_CASE("unary minus negates its operand", "[arithmetic][unary]")
{
    CHECK_THAT(eval_of("-5"), WithinAbs(-5.0, k_epsilon));
    CHECK_THAT(eval_of("-(2 + 3)"), WithinAbs(-5.0, k_epsilon));
}

TEST_CASE("unary plus is a no-op", "[arithmetic][unary]")
{
    CHECK_THAT(eval_of("+5"), WithinAbs(5.0, k_epsilon));
}

TEST_CASE("repeated unary signs fold correctly", "[arithmetic][unary]")
{
    CHECK_THAT(eval_of("--5"), WithinAbs(5.0, k_epsilon));
    CHECK_THAT(eval_of("---5"), WithinAbs(-5.0, k_epsilon));
    CHECK_THAT(eval_of("-+-5"), WithinAbs(5.0, k_epsilon));
    CHECK_THAT(eval_of("+-+-+5"), WithinAbs(5.0, k_epsilon));
}

TEST_CASE("unary minus binds looser than exponentiation on the left operand", "[arithmetic][unary][precedence]")
{
    // parse_unary consumes any leading sign and applies it *after* parsing
    // a full power expression, so '^' binds tighter than a leading unary
    // minus: -2^2 is -(2^2) = -4, not (-2)^2 = 4.
    CHECK_THAT(eval_of("-2^2"), WithinAbs(-4.0, k_epsilon));
    CHECK_THAT(eval_of("(-2)^2"), WithinAbs(4.0, k_epsilon));
}

TEST_CASE("binary minus following a number is subtraction, not double negation", "[arithmetic][unary]")
{
    CHECK_THAT(eval_of("5 - -3"), WithinAbs(8.0, k_epsilon));
    CHECK_THAT(eval_of("5 - - -3"), WithinAbs(2.0, k_epsilon));
}

TEST_CASE("modulo follows fmod semantics, including sign of the dividend", "[arithmetic]")
{
    CHECK_THAT(eval_of("-7 % 3"), WithinAbs(std::fmod(-7.0, 3.0), k_epsilon));
    CHECK_THAT(eval_of("7 % -3"), WithinAbs(std::fmod(7.0, -3.0), k_epsilon));
}

TEST_CASE("division by zero yields IEEE infinities or NaN rather than an error", "[arithmetic][edge-cases]")
{
    CHECK(std::isinf(eval_of("1 / 0")));
    CHECK(std::isnan(eval_of("0 / 0")));
    CHECK(std::isinf(eval_of("-1 / 0")));
}

// ---------------------------------------------------------------------
// Variables
// ---------------------------------------------------------------------

TEST_CASE("a bound variable is read at evaluation time", "[variables]")
{
    double x = 5.0;
    const Variable vars[] = {Variable::bind("x", x)};
    CHECK_THAT(eval_of("x * 2", vars), WithinAbs(10.0, k_epsilon));
}

TEST_CASE("re-evaluating a compiled expression sees updated variable values", "[variables]")
{
    double x = 1.0;
    const Variable vars[] = {Variable::bind("x", x)};
    auto compiled = compile("x * x", vars);
    REQUIRE(compiled.has_value());

    CHECK_THAT(compiled->eval(), WithinAbs(1.0, k_epsilon));
    x = 4.0;
    CHECK_THAT(compiled->eval(), WithinAbs(16.0, k_epsilon));
    x = -3.0;
    CHECK_THAT(compiled->eval(), WithinAbs(9.0, k_epsilon));
}

TEST_CASE("multiple variables can be combined in one expression", "[variables]")
{
    double x = 2.0;
    double y = 3.0;
    const Variable vars[] = {Variable::bind("x", x), Variable::bind("y", y)};
    CHECK_THAT(eval_of("x^2 + y^2", vars), WithinAbs(13.0, k_epsilon));
}

TEST_CASE("an unknown identifier is a compile error", "[variables][errors]")
{
    ParseError err = error_of("x + 1");
    CHECK(err.kind == ErrorKind::UnknownIdentifier);
}

TEST_CASE("a user variable shadows a built-in of the same name", "[variables]")
{
    double e = 100.0;
    const Variable vars[] = {Variable::bind("e", e)};
    CHECK_THAT(eval_of("e", vars), WithinAbs(100.0, k_epsilon));
}

// ---------------------------------------------------------------------
// Built-in functions
// ---------------------------------------------------------------------

TEST_CASE("zero-argument built-ins may be called with or without parentheses", "[builtins][zero-arity]")
{
    CHECK_THAT(eval_of("pi"), WithinRel(std::numbers::pi_v<double>));
    CHECK_THAT(eval_of("pi()"), WithinRel(std::numbers::pi_v<double>));
    CHECK_THAT(eval_of("e"), WithinRel(std::numbers::e_v<double>));
}

TEST_CASE("a zero-arity call with a non-empty argument list is an error", "[builtins][zero-arity][errors]")
{
    auto result = interpret("pi(1)");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnbalancedParentheses);
}

TEST_CASE("one-argument built-ins bind as tightly as unary minus", "[builtins][one-arity]")
{
    CHECK_THAT(eval_of("sqrt 9"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("sqrt(9)"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("sqrt 9 + 1"), WithinAbs(4.0, k_epsilon));   // (sqrt 9) + 1
    CHECK_THAT(eval_of("sqrt(9 + 16)"), WithinAbs(5.0, k_epsilon)); // sqrt(9 + 16)
}

TEST_CASE("standard one-argument math functions", "[builtins][one-arity]")
{
    CHECK_THAT(eval_of("abs(-5)"), WithinAbs(5.0, k_epsilon));
    CHECK_THAT(eval_of("floor(2.7)"), WithinAbs(2.0, k_epsilon));
    CHECK_THAT(eval_of("ceil(2.1)"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("ln(e())"), WithinAbs(1.0, k_epsilon));
    CHECK_THAT(eval_of("log10(1000)"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("log(1000)"), WithinAbs(3.0, k_epsilon)); // alias for log10 in this port
    CHECK_THAT(eval_of("sin(0)"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("cos(0)"), WithinAbs(1.0, k_epsilon));
    CHECK_THAT(eval_of("exp(0)"), WithinAbs(1.0, k_epsilon));
    CHECK_THAT(eval_of("tan(0)"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("sinh(0)"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("cosh(0)"), WithinAbs(1.0, k_epsilon));
    CHECK_THAT(eval_of("tanh(0)"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("asin(0)"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("acos(1)"), WithinAbs(0.0, k_epsilon));
    CHECK_THAT(eval_of("atan(0)"), WithinAbs(0.0, k_epsilon));
}

TEST_CASE("fac computes factorial and saturates to infinity on overflow", "[builtins][one-arity]")
{
    CHECK_THAT(eval_of("fac(0)"), WithinAbs(1.0, k_epsilon));
    CHECK_THAT(eval_of("fac(5)"), WithinAbs(120.0, k_epsilon));
    CHECK(std::isnan(eval_of("fac(-1)")));
    CHECK(std::isinf(eval_of("fac(1e20)")));
}

TEST_CASE("two-argument built-ins require a parenthesized argument list", "[builtins][two-arity]")
{
    CHECK_THAT(eval_of("pow(2, 10)"), WithinAbs(1024.0, k_epsilon));
    CHECK_THAT(eval_of("atan2(1, 1)"), WithinAbs(std::atan2(1.0, 1.0), k_epsilon));
}

TEST_CASE("min and max compute standard min and max", "[builtins][two-arity]")
{
    CHECK_THAT(eval_of("min(2, 10)"), WithinAbs(2.0, k_epsilon));
    CHECK_THAT(eval_of("max(2, 10)"), WithinAbs(10.0, k_epsilon));
}

TEST_CASE("ncr and npr compute combinations and permutations", "[builtins][two-arity]")
{
    CHECK_THAT(eval_of("ncr(5, 2)"), WithinAbs(10.0, k_epsilon));
    CHECK_THAT(eval_of("npr(5, 2)"), WithinAbs(20.0, k_epsilon));
    CHECK(std::isnan(eval_of("ncr(2, 5)"))); // r > n
}

TEST_CASE("a two-arity call missing its parentheses is an error", "[builtins][two-arity][errors]")
{
    auto result = interpret("pow 2, 3");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::MissingArgument);
}

TEST_CASE("a two-arity call with too few arguments is an error", "[builtins][two-arity][errors]")
{
    auto result = interpret("pow(2)");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::MissingArgument);
}

TEST_CASE("a two-arity call with too many arguments is an error", "[builtins][two-arity][errors]")
{
    auto result = interpret("pow(2, 3, 4)");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::MissingArgument);
}

// TEST_CASE("clamp computes std::clamp", "[builtins][three-arity]")
// {
//     CHECK_THAT(eval_of("clamp(-1, 0, 10)"), WithinAbs(std::clamp(-1.0, 0.0, 10.0), k_epsilon));
//     CHECK_THAT(eval_of("clamp(5, 0, 10)"), WithinAbs(std::clamp(5.0, 0.0, 10.0), k_epsilon));
//     CHECK_THAT(eval_of("clamp(15, 0, 10)"), WithinAbs(std::clamp(15.0, 0.0, 10.0), k_epsilon));
// }

TEST_CASE("built-in names are case sensitive", "[builtins][lexer]")
{
    auto result = interpret("SQRT(4)");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnknownIdentifier);
}

// ---------------------------------------------------------------------
// User-registered functions
// ---------------------------------------------------------------------

namespace
{
double double_it(double a) noexcept
{
    return a * 2.0;
}

double add3(double a, double b, double c) noexcept
{
    return a + b + c;
}

double always_seven() noexcept
{
    return 7.0;
}

double stateful_offset(std::span<const double> args, void* context) noexcept
{
    const double offset = *static_cast<double*>(context);
    return args[0] + offset;
}
} // namespace

TEST_CASE("a one-argument user function can be registered via Variable::function", "[user-functions]")
{
    const Variable vars[] = {Variable::function<&double_it>("dbl")};
    CHECK_THAT(eval_of("dbl(21)", vars), WithinAbs(42.0, k_epsilon));
    CHECK_THAT(eval_of("dbl 21", vars), WithinAbs(42.0, k_epsilon)); // tight binding like built-ins
}

TEST_CASE("a zero-argument user function can be registered via Variable::function", "[user-functions]")
{
    const Variable vars[] = {Variable::function<&always_seven>("seven")};
    CHECK_THAT(eval_of("seven", vars), WithinAbs(7.0, k_epsilon));
    CHECK_THAT(eval_of("seven()", vars), WithinAbs(7.0, k_epsilon));
}

TEST_CASE("a three-argument user function can be registered via Variable::function", "[user-functions]")
{
    const Variable vars[] = {Variable::function<&add3>("add3")};
    CHECK_THAT(eval_of("add3(1, 2, 3)", vars), WithinAbs(6.0, k_epsilon));
}

TEST_CASE("a user function shadows a built-in of the same name", "[user-functions]")
{
    const Variable vars[] = {Variable::function<&double_it>("sqrt")};
    CHECK_THAT(eval_of("sqrt(21)", vars), WithinAbs(42.0, k_epsilon));
}

TEST_CASE("Variable::closure carries user-supplied state through evaluation", "[user-functions][closure]")
{
    double offset = 100.0;
    const Variable vars[] = {
        Variable::closure("offset_by", &stateful_offset, 1, &offset, /*pure=*/false),
    };
    CHECK_THAT(eval_of("offset_by(5)", vars), WithinAbs(105.0, k_epsilon));

    offset = -1.0;
    // Re-evaluating re-reads the same context pointer, so the new offset is seen.
    auto compiled = compile("offset_by(5)", vars);
    REQUIRE(compiled.has_value());
    CHECK_THAT(compiled->eval(), WithinAbs(4.0, k_epsilon));
}

TEST_CASE("a closure registered with arity above kMaxArity fails to compile", "[user-functions][errors]")
{
    double ctx = 0.0;
    const Variable vars[] = {
        Variable::closure("toomany", &stateful_offset, static_cast<std::uint8_t>(k_max_arity + 1), &ctx),
    };
    auto result = interpret("toomany(1, 2, 3, 4, 5, 6, 7, 8, 9)", vars);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::ArityTooLarge);
}

TEST_CASE("an impure closure is never constant-folded away", "[user-functions][closure][optimize]")
{
    // This is a behavioral, not structural, check: an impure call must be
    // re-evaluated every time rather than folded once at compile time.
    int calls = 0;
    struct Counter
    {
        int* calls;
    } counter{&calls};

    static int* s_calls = &calls; // avoid needing a capturing function pointer
    auto counting_fn = [](std::span<const double> args, void* /*context*/) noexcept -> double {
        ++(*s_calls);
        return args[0];
    };
    (void)counter;

    const Variable vars[] = {
        Variable::closure("count", counting_fn, 1, nullptr, /*pure=*/false),
    };
    auto compiled = compile("count(1)", vars);
    REQUIRE(compiled.has_value());
    CHECK(calls == 0); // not evaluated at compile time
    compiled->eval();
    compiled->eval();
    CHECK(calls == 2); // evaluated once per eval() call
}

// ---------------------------------------------------------------------
// Comma / list operator
// ---------------------------------------------------------------------

TEST_CASE("the top-level comma operator evaluates to its last operand", "[list]")
{
    CHECK_THAT(eval_of("1, 2, 3"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("(1, 2, 3)"), WithinAbs(3.0, k_epsilon));
}

TEST_CASE("a parenthesized comma expression can be used as an operand", "[list]")
{
    CHECK_THAT(eval_of("(1, 2, 3) + 10"), WithinAbs(13.0, k_epsilon));
}

// ---------------------------------------------------------------------
// Error kinds
// ---------------------------------------------------------------------

TEST_CASE("an unbalanced opening parenthesis is reported", "[errors]")
{
    ParseError err = error_of("(1 + 2");
    CHECK(err.kind == ErrorKind::UnbalancedParentheses);
}

TEST_CASE("trailing input after a complete expression is reported", "[errors]")
{
    ParseError err = error_of("1 + 2)");
    // The stray ')' cannot start a new base expression, so parse_expr stops
    // after "1 + 2" and the parser reports the leftover input.
    CHECK(err.kind == ErrorKind::TrailingInput);
}

TEST_CASE("an incomplete binary expression is a missing-token / unexpected-token error", "[errors]")
{
    auto result = interpret("1 +");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnexpectedToken);
}

TEST_CASE("a stray operator at the start of an otherwise-empty position is unexpected", "[errors]")
{
    auto result = interpret("*5");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnexpectedToken);
}

TEST_CASE("an unrecognized character is reported as an unexpected token", "[errors][lexer]")
{
    // "1 @ 2" does NOT exercise this path: "1" is already a complete
    // expression, so parse_expr stops there and the stray "@ 2" is reported
    // as TrailingInput (see "trailing input after a complete expression is
    // reported", below), the same way "1 + 2)" is. To actually reach
    // parse_base() while it needs a token, the bad character has to sit
    // where an operand is expected.
    auto result = interpret("1 + @ 2");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnexpectedToken);
}

TEST_CASE("a malformed identifier that matches no variable or function is unknown", "[errors]")
{
    ParseError err = error_of("foobar(1)");
    CHECK(err.kind == ErrorKind::UnknownIdentifier);
}

TEST_CASE("error position points at the offending character", "[errors][position]")
{
    // "1 + " -- the '+' consumes fine, then the parser wants a base and
    // finds End at offset 4.
    auto result = interpret("1 + ");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().position == 4);
}

TEST_CASE("error position for an unbalanced parenthesis points at the missing close", "[errors][position]")
{
    auto result = interpret("(1 + 2");
    REQUIRE_FALSE(result.has_value());
    // Lexer has reached End (offset 6) looking for ')'.
    CHECK(result.error().position == 6);
}

TEST_CASE("the first error encountered is the one reported", "[errors]")
{
    // Two problems exist here (unknown identifier, then trailing content);
    // Parser::fail only ever records the first one.
    auto result = interpret("bogus + )");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::UnknownIdentifier);
}

// ---------------------------------------------------------------------
// Expression: move semantics, to_string, empty state
// ---------------------------------------------------------------------

TEST_CASE("Expression can be move-constructed and remains evaluable", "[expression][move]")
{
    auto compiled = compile("2 + 2");
    REQUIRE(compiled.has_value());
    Expression moved(std::move(*compiled));
    CHECK_THAT(moved.eval(), WithinAbs(4.0, k_epsilon));
}

TEST_CASE("Expression can be move-assigned and remains evaluable", "[expression][move]")
{
    auto a = compile("2 + 2");
    auto b = compile("10 * 10");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    Expression expr_a(std::move(*a));
    Expression expr_b(std::move(*b));
    expr_a = std::move(expr_b);
    CHECK_THAT(expr_a.eval(), WithinAbs(100.0, k_epsilon));
}

TEST_CASE("to_string renders a non-empty tree for a compiled expression", "[expression][to_string]")
{
    // "1 + 2 * 3" is not a good fixture here: it is fully constant and
    // pure, so optimize() legitimately collapses it to a single Constant
    // node (see "to_string reflects the constant-folded tree...", below) --
    // there would be no "call" left to find. Route a variable through the
    // expression instead so the call node survives folding.
    double x = 3.0;
    const Variable vars[] = {Variable::bind("x", x)};
    auto compiled = compile("1 + 2 * x", vars);
    REQUIRE(compiled.has_value());
    const std::string dump = compiled->to_string();
    CHECK_FALSE(dump.empty());
    CHECK_THAT(dump, Catch::Matchers::ContainsSubstring("call/2"));
}

TEST_CASE("to_string reflects the constant-folded tree for a pure sub-expression", "[expression][to_string][optimize]")
{
    // "2 + 3" is a pure call over two constants, so optimize() collapses it
    // to a single constant node before to_string() ever sees it.
    auto compiled = compile("2 + 3");
    REQUIRE(compiled.has_value());
    const std::string dump = compiled->to_string();
    CHECK_THAT(dump, Catch::Matchers::ContainsSubstring("5"));
    CHECK_THAT(dump, !Catch::Matchers::ContainsSubstring("call"));
}

// ---------------------------------------------------------------------
// Constant folding correctness (behavioral)
// ---------------------------------------------------------------------

TEST_CASE("constant folding does not change the evaluated result", "[optimize]")
{
    CHECK_THAT(eval_of("(2 + 3) * (4 - 1) / 5"), WithinAbs(3.0, k_epsilon));
    CHECK_THAT(eval_of("sqrt(16) + pow(2, 3)"), WithinAbs(12.0, k_epsilon));
}

TEST_CASE("a call mixing constant and variable arguments is not folded away", "[optimize][variables]")
{
    double x = 3.0;
    const Variable vars[] = {Variable::bind("x", x)};
    auto compiled = compile("x + 2", vars);
    REQUIRE(compiled.has_value());
    CHECK_THAT(compiled->eval(), WithinAbs(5.0, k_epsilon));
    x = 10.0;
    CHECK_THAT(compiled->eval(), WithinAbs(12.0, k_epsilon));
}

// ---------------------------------------------------------------------
// Larger composite expressions
// ---------------------------------------------------------------------

TEST_CASE("a realistic composite expression evaluates correctly", "[integration]")
{
    double x = 2.0;
    double y = 5.0;
    const Variable vars[] = {Variable::bind("x", x), Variable::bind("y", y)};
    const double expected = std::sqrt(x * x + y * y) + std::sin(x) * 2.0 - 1.0;
    CHECK_THAT(eval_of("sqrt(x^2 + y^2) + sin(x) * 2 - 1", vars), WithinAbs(expected, k_epsilon));
}

TEST_CASE("deeply nested parentheses still parse correctly", "[integration]")
{
    // (((((1 + 1) * 2) - 1) * 3) + 1) = ((1+1)*2 - 1) * 3 + 1
    //                                  = (4 - 1) * 3 + 1 = 3 * 3 + 1 = 10.
    // The original expected value here (16.0) was simply an arithmetic
    // slip, not a bug in the parser.
    CHECK_THAT(eval_of("(((((1 + 1) * 2) - 1) * 3) + 1)"), WithinAbs(10.0, k_epsilon));
}