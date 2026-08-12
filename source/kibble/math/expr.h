/**
 * @brief kb::expr: Tiny recursive descent parser and evaluation engine in C++.
 *
 * This is a C++ port of the original TinyExpr library by
 * Lewis Van Winkle (http://CodePlea.com), which is released
 * under the zlib license. The public API is redesigned for C++,
 * but the expression grammar and the set of built-in functions
 * are kept compatible with the original.
 * A few built-ins have been added too.
 *
 * This port replaces TinyExpr's C API (raw te_expr* trees, manual
 * te_free, and an int* error code) with a move-only Expression type
 * and std::expected<Expression, ParseError>, so a failed compile
 * carries a position, an ErrorKind, and a human-readable message
 * instead of just an offset. Custom functions are registered through
 * Variable::function<&Fn>, which deduces arity from the function
 * pointer's signature at compile time instead of relying on runtime
 * arity flags like TE_FUNCTION2. Native functions take their
 * arguments as std::span<const double> rather than a fixed set of
 * arity-specific function pointer types. Arc hyperbolic trig functions,
 * cbrt for cube root, erf and erfc, as well as min, max and clamp,
 * have been added to the built-in set. One behavioral
 * difference from the original: exponentiation is right-associative
 * here (a^b^c == a^(b^c)), whereas TinyExpr is left-associative by
 * default (a^b^c == (a^b)^c) unless built with TE_POW_FROM_RIGHT.
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

/// @brief A tiny recursive descent parser and evaluator for math expressions.
namespace kb::expr
{

namespace detail
{
struct Node;
}

/// @brief The category of problem found while compiling an expression.
enum class ErrorKind : std::uint8_t
{
    EmptyExpression,       ///< The expression string is empty.
    UnexpectedToken,       ///< A token did not fit the grammar at this point.
    UnknownIdentifier,     ///< A name is not a known variable or function.
    UnbalancedParentheses, ///< A closing parenthesis is missing.
    MissingArgument,       ///< A function call is missing one or more arguments.
    TrailingInput,         ///< Extra characters follow an otherwise complete expression.
    ArityTooLarge,         ///< A function was registered with more arguments than supported.
};

/**
 * @brief Describes why compiling an expression failed.
 *
 * The @ref position field is a zero-based offset into the original
 * expression string, pointing at the character where the problem
 * was detected.
 */
struct ParseError
{
    std::size_t position; ///< Offset into the expression where the error was found.
    ErrorKind kind;       ///< Machine-readable category of the error.
    std::string message;  ///< Human-readable, ready-to-display description.
};

/// @brief The largest number of arguments a native function may take.
inline constexpr std::size_t k_max_arity = 8;

/**
 * @brief Signature for every native function callable from an expression.
 *
 * `args` holds the already-evaluated arguments, and `context` is
 * whatever pointer was supplied when the function was registered
 * (see @ref Variable::closure). Stateless functions can ignore it.
 */
using NativeFunction = double (*)(std::span<const double> args, void* context);

class Expression;

/**
 * @brief Binds a name to a variable, a constant function, or a stateful function.
 *
 * Build instances with the static factory functions below instead of
 * constructing a Variable directly.
 */
class Variable
{
public:
    /// @brief Binds `name` to a variable whose value is read at evaluation time.
    static Variable bind(std::string_view name, const double& value) noexcept;

    /// @brief Binds `name` to a zero-argument function.
    template <double (*Fn)()>
    static Variable function(std::string_view name, bool pure = true) noexcept;

    /// @brief Binds `name` to a one-argument function.
    template <double (*Fn)(double)>
    static Variable function(std::string_view name, bool pure = true) noexcept;

    /// @brief Binds `name` to a two-argument function.
    template <double (*Fn)(double, double)>
    static Variable function(std::string_view name, bool pure = true) noexcept;

    /// @brief Binds `name` to a three-argument function.
    template <double (*Fn)(double, double, double)>
    static Variable function(std::string_view name, bool pure = true) noexcept;

    /// @brief Binds `name` to a four-argument function.
    template <double (*Fn)(double, double, double, double)>
    static Variable function(std::string_view name, bool pure = true) noexcept;

    /**
     * @brief Binds `name` to a function that carries extra state.
     *
     * `context` is handed back to `fn` on every call, so one function
     * pointer can serve many bindings, for example one entry per
     * element of an array of values. `arity` must not exceed
     * @ref kMaxArity.
     */
    static Variable closure(std::string_view name, NativeFunction fn, std::uint8_t arity, void* context,
                            bool pure = false) noexcept;

private:
    friend class Expression;
    friend class Lexer;
    friend std::expected<Expression, ParseError> compile(std::string_view, std::span<const Variable>);

    Variable() noexcept = default;

    std::string_view name_;
    const double* bound_ = nullptr;
    NativeFunction function_ = nullptr;
    void* context_ = nullptr;
    std::uint8_t arity_ = 0;
    bool pure_ = false;
};

/**
 * @brief A compiled math expression, ready to be evaluated.
 *
 * Expression is move-only: it owns a syntax tree, so it cannot be
 * copied cheaply. Build one with @ref compile, then call
 * @ref Expression::eval as many times as needed, for example after
 * changing the values behind bound variables.
 */
class Expression
{
public:
    Expression(Expression&& other) noexcept;
    Expression& operator=(Expression&& other) noexcept;
    Expression(const Expression&) = delete;
    Expression& operator=(const Expression&) = delete;
    ~Expression();

    /// @brief Evaluates the expression using the current values of any bound variables.
    double eval() const noexcept;

    /// @brief Renders the syntax tree as indented text, for debugging.
    std::string to_string() const;

private:
    friend std::expected<Expression, ParseError> compile(std::string_view, std::span<const Variable>);

    explicit Expression(std::unique_ptr<detail::Node> root) noexcept;

    std::unique_ptr<detail::Node> root_;
};

/**
 * @brief Parses `expression` and binds `variables` for later evaluation.
 *
 * Compiling is where all parsing and identifier lookup happens, so
 * this is the only call that can fail. `variables` only needs to
 * stay valid for the duration of this call; the compiled expression
 * keeps its own pointers to the bound doubles, not to the `Variable`
 * descriptors.
 */
[[nodiscard]] std::expected<Expression, ParseError> compile(std::string_view expression,
                                                            std::span<const Variable> variables = {});

/// @brief Parses and immediately evaluates `expression`. A convenience for one-shot use.
[[nodiscard]] std::expected<double, ParseError> interpret(std::string_view expression,
                                                          std::span<const Variable> variables = {});

namespace detail
{

/// @internal @brief Adapts a zero-argument function to @ref NativeFunction.
template <double (*Fn)()>
double invoke0(std::span<const double> /*args*/, void* /*context*/) noexcept
{
    return Fn();
}

/// @internal @brief Adapts a one-argument function to @ref NativeFunction.
template <double (*Fn)(double)>
double invoke1(std::span<const double> args, void* /*context*/) noexcept
{
    return Fn(args[0]);
}

/// @internal @brief Adapts a two-argument function to @ref NativeFunction.
template <double (*Fn)(double, double)>
double invoke2(std::span<const double> args, void* /*context*/) noexcept
{
    return Fn(args[0], args[1]);
}

/// @internal @brief Adapts a three-argument function to @ref NativeFunction.
template <double (*Fn)(double, double, double)>
double invoke3(std::span<const double> args, void* /*context*/) noexcept
{
    return Fn(args[0], args[1], args[2]);
}

/// @internal @brief Adapts a four-argument function to @ref NativeFunction.
template <double (*Fn)(double, double, double, double)>
double invoke4(std::span<const double> args, void* /*context*/) noexcept
{
    return Fn(args[0], args[1], args[2], args[3]);
}

} // namespace detail

template <double (*Fn)()>
Variable Variable::function(std::string_view name, bool pure) noexcept
{
    return Variable::closure(name, &detail::invoke0<Fn>, 0, nullptr, pure);
}

template <double (*Fn)(double)>
Variable Variable::function(std::string_view name, bool pure) noexcept
{
    return Variable::closure(name, &detail::invoke1<Fn>, 1, nullptr, pure);
}

template <double (*Fn)(double, double)>
Variable Variable::function(std::string_view name, bool pure) noexcept
{
    return Variable::closure(name, &detail::invoke2<Fn>, 2, nullptr, pure);
}

template <double (*Fn)(double, double, double)>
Variable Variable::function(std::string_view name, bool pure) noexcept
{
    return Variable::closure(name, &detail::invoke3<Fn>, 3, nullptr, pure);
}

template <double (*Fn)(double, double, double, double)>
Variable Variable::function(std::string_view name, bool pure) noexcept
{
    return Variable::closure(name, &detail::invoke4<Fn>, 4, nullptr, pure);
}

} // namespace kb::expr