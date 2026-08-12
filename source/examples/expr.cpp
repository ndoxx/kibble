/*
 * Example program for kb::expr.
 *
 * This program shows one-shot evaluation, compiled expressions with
 * bound variables, custom stateless and stateful functions, the
 * built-in math library, syntax tree dumps, and parse error handling.
 */
#include "kibble/math/expr.h"

#include <array>
#include <limits>
#include <span>
#include <vector>

#include <fmt/core.h>

namespace
{

// Returns a display name for a kb::expr::ErrorKind value.
const char* error_kind_name(kb::expr::ErrorKind kind)
{
    switch (kind)
    {
    case kb::expr::ErrorKind::EmptyExpression:
        return "EmptyExpression";
    case kb::expr::ErrorKind::UnexpectedToken:
        return "UnexpectedToken";
    case kb::expr::ErrorKind::UnknownIdentifier:
        return "UnknownIdentifier";
    case kb::expr::ErrorKind::UnbalancedParentheses:
        return "UnbalancedParentheses";
    case kb::expr::ErrorKind::MissingArgument:
        return "MissingArgument";
    case kb::expr::ErrorKind::TrailingInput:
        return "TrailingInput";
    case kb::expr::ErrorKind::ArityTooLarge:
        return "ArityTooLarge";
    }
    return "Unknown";
}

// Prints a parse error, with a caret under the offending position.
void print_error(std::string_view expression, const kb::expr::ParseError& err)
{
    fmt::print("  {}\n", expression);
    fmt::print("  {:>{}}^\n", "", err.position);
    fmt::print("  {}: {}\n", error_kind_name(err.kind), err.message);
}

// A plain function. Use this form for a stateless, pure function.
double square(double x) noexcept
{
    return x * x;
}

/*
 * NOTE(ndx):
 * This function reads from a std::vector<double> passed in as the
 * `context` pointer. Register it with Variable::closure. Use this
 * form when a function needs access to external data, such as an
 * array to index into at evaluation time.
 */
double array_lookup(std::span<const double> args, void* context) noexcept
{
    const auto* data = static_cast<const std::vector<double>*>(context);
    const auto index = static_cast<std::size_t>(args[0]);
    if (index >= data->size())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (*data)[index];
}

} // namespace

int main()
{
    // A one-shot expression needs no variables.
    fmt::print("== One-shot evaluation ==\n");
    const std::string_view basic = "2 + 3 * 4 - 5 / (1 + 1)";
    if (auto result = kb::expr::interpret(basic))
    {
        fmt::print("{} = {:.6g}\n\n", basic, *result);
    }

    // Compile once, then re-evaluate after changing the bound variable.
    fmt::print("== Bound variable ==\n");
    double x = 0.0;
    const std::array<kb::expr::Variable, 1> vars_x = {kb::expr::Variable::bind("x", x)};
    if (auto compiled = kb::expr::compile("x^2 - 2*x + 1", vars_x))
    {
        for (x = 0.0; x <= 3.0; x += 1.0)
        {
            fmt::print("x = {:.6g} -> {:.6g}\n", x, compiled->eval());
        }
        fmt::print("\n");
    }

    // A custom, pure function registered with Variable::function.
    fmt::print("== Custom function ==\n");
    const std::array<kb::expr::Variable, 1> vars_square = {kb::expr::Variable::function<&square>("square")};
    if (auto result = kb::expr::interpret("square(3) + square(4)", vars_square))
    {
        fmt::print("square(3) + square(4) = {:.6g}\n\n", *result);
    }

    // A custom, stateful function registered with Variable::closure.
    fmt::print("== Stateful function ==\n");
    std::vector<double> table = {10.0, 20.0, 30.0, 40.0};
    const std::array<kb::expr::Variable, 1> vars_elem = {
        kb::expr::Variable::closure("elem", &array_lookup, 1, &table)};
    if (auto result = kb::expr::interpret("elem(0) + elem(3)", vars_elem))
    {
        fmt::print("elem(0) + elem(3) = {:.6g}\n\n", *result);
    }

    // A built-in function and a built-in constant.
    fmt::print("== Built-ins ==\n");
    if (auto result = kb::expr::interpret("sqrt(2) * pi"))
    {
        fmt::print("sqrt(2) * pi = {:.6g}\n\n", *result);
    }

    const std::string_view builtin = "exp(0)";
    if (auto result = kb::expr::interpret(builtin))
    {
        fmt::print("{} = {:.6g}\n\n", builtin, *result);
    }

    /*
     * NOTE(ndx):
     * A pure sub-expression with no variables folds to a constant at
     * compile time, so it will not appear as a call node here. This
     * expression keeps "x" unfolded to show a call node with children.
     */
    fmt::print("== Syntax tree ==\n");
    if (auto compiled = kb::expr::compile("x + 2 * 3", vars_x))
    {
        fmt::print("{}\n", compiled->to_string());
    }

    // compile() and interpret() return std::expected, so a failed
    // parse never throws.
    fmt::print("== Parse errors ==\n");
    const std::array<std::string_view, 3> bad_expressions = {
        "2 + ",
        "(1 + 2",
        "unknown_name + 1",
    };
    for (const auto& expression : bad_expressions)
    {
        if (auto compiled = kb::expr::compile(expression); !compiled)
        {
            print_error(expression, compiled.error());
            fmt::print("\n");
        }
    }

    return 0;
}