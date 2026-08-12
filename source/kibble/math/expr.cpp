/*
 * kb::expr: Tiny recursive descent parser and evaluation engine in C++.
 *
 * This file implements the grammar described below. It is a direct
 * descendant of the original TinyExpr grammar by Lewis Van Winkle:
 *
 *   <list>   = <expr> {"," <expr>}
 *   <expr>   = <term> {("+" | "-") <term>}
 *   <term>   = <factor> {("*" | "/" | "%") <factor>}
 *   <factor> = <unary>
 *   <unary>  = {("-" | "+")} <power>
 *   <power>  = <base> {"^" <unary>}
 *   <base>   = <constant> | <variable> | <function-0> {"(" ")"}
 *            | <function-1> <unary> | <function-X> "(" <expr> {"," <expr>} ")"
 *            | "(" <list> ")"
 *
 * NOTE(ndx):
 * '^' binds tighter than a leading unary minus, so "-2^2" is "-(2^2)"
 * (-4), not "(-2)^2" (4). A sign on the exponent itself, as in "2^-2",
 * is still accepted without parentheses. Exponentiation associates
 * right-to-left, matching one of the two modes the original library
 * offered behind a compile-time macro (the other, left-to-right, is not
 * available: parenthesize explicitly if you need it).
 */
#include "kibble/math/expr.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

namespace kb::expr
{

namespace detail
{

/// @internal @brief One node of a compiled expression's syntax tree.
struct Node
{
    /// @internal @brief What a node represents.
    enum class Kind : std::uint8_t
    {
        Constant, ///< A literal number, known at compile time.
        Variable, ///< A value read from a bound `double` at evaluation time.
        Call,     ///< A native function applied to the node's children.
    };

    Kind kind = Kind::Constant;
    double value = 0.0;
    const double* bound = nullptr;
    NativeFunction function = nullptr;
    void* context = nullptr;
    bool pure = false;
    std::vector<std::unique_ptr<Node>> children;
};

} // namespace detail

namespace
{

using detail::Node;

/// @internal @brief Evaluates one syntax tree node, recursing into its children.
double evaluate(const Node& node) noexcept
{
    switch (node.kind)
    {
    case Node::Kind::Constant:
        return node.value;
    case Node::Kind::Variable:
        return *node.bound;
    case Node::Kind::Call: {
        std::array<double, k_max_arity> args{};
        const std::size_t arity = node.children.size();
        for (std::size_t ii = 0; ii < arity; ++ii)
        {
            args[ii] = evaluate(*node.children[ii]);
        }
        return node.function(std::span<const double>(args.data(), arity), node.context);
    }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

/**
 * @internal
 * @brief Folds constant sub-expressions in place.
 *
 * A call node collapses to a constant when it is flagged pure and
 * every one of its arguments is itself a constant after folding.
 */
void optimize(Node& node) noexcept
{
    if (node.kind != Node::Kind::Call)
    {
        return;
    }
    bool all_constant = node.pure;
    for (auto& child : node.children)
    {
        optimize(*child);
        if (child->kind != Node::Kind::Constant)
        {
            all_constant = false;
        }
    }
    if (all_constant)
    {
        const double value = evaluate(node);
        node.children.clear();
        node.kind = Node::Kind::Constant;
        node.value = value;
        node.function = nullptr;
        node.context = nullptr;
    }
}

/// @internal @brief Appends an indented, human-readable dump of a node and its children.
void print_node(const Node& node, std::size_t depth, std::string& out)
{
    out.append(depth * 2, ' ');
    switch (node.kind)
    {
    case Node::Kind::Constant:
        out += std::to_string(node.value);
        out += '\n';
        return;
    case Node::Kind::Variable:
        out += "variable\n";
        return;
    case Node::Kind::Call:
        out += "call/" + std::to_string(node.children.size()) + '\n';
        for (const auto& child : node.children)
        {
            print_node(*child, depth + 1, out);
        }
        return;
    }
}

// ---------------------------------------------------------------------
// Built-in math functions.
// ---------------------------------------------------------------------

// clang-format off
double math_abs(double a) noexcept                       { return std::fabs(a); }
double math_acos(double a) noexcept                      { return std::acos(a); }
double math_acosh(double a) noexcept                     { return std::acosh(a); }
double math_asin(double a) noexcept                      { return std::asin(a); }
double math_asinh(double a) noexcept                     { return std::asinh(a); }
double math_atan(double a) noexcept                      { return std::atan(a); }
double math_atan2(double a, double b) noexcept           { return std::atan2(a, b); }
double math_atanh(double a) noexcept                     { return std::atanh(a); }
double math_cbrt(double a) noexcept                      { return std::cbrt(a); }
double math_ceil(double a) noexcept                      { return std::ceil(a); }
double math_clamp(double a, double b, double c) noexcept { return std::clamp(a, b, c); }
double math_cos(double a) noexcept                       { return std::cos(a); }
double math_cosh(double a) noexcept                      { return std::cosh(a); }
double math_e() noexcept                                 { return std::numbers::e_v<double>; }
double math_exp(double a) noexcept                       { return std::exp(a); }
double math_exp2(double a) noexcept                      { return std::exp2(a); }
double math_erf(double a) noexcept                       { return std::erf(a); }
double math_erfc(double a) noexcept                      { return std::erfc(a); }
double math_floor(double a) noexcept                     { return std::floor(a); }
double math_ln(double a) noexcept                        { return std::log(a); }
double math_log10(double a) noexcept                     { return std::log10(a); }
double math_log2(double a) noexcept                      { return std::log2(a); }
double math_max(double a, double b) noexcept             { return std::max(a, b); }
double math_min(double a, double b) noexcept             { return std::min(a, b); }
double math_pi() noexcept                                { return std::numbers::pi_v<double>; }
double math_pow(double a, double b) noexcept             { return std::pow(a, b); }
double math_sin(double a) noexcept                       { return std::sin(a); }
double math_sinh(double a) noexcept                      { return std::sinh(a); }
double math_sqrt(double a) noexcept                      { return std::sqrt(a); }
double math_tan(double a) noexcept                       { return std::tan(a); }
double math_tanh(double a) noexcept                      { return std::tanh(a); }
// clang-format on

/// @internal @brief Factorial, saturating to infinity instead of overflowing.
double math_fac(double a) noexcept
{
    if (a < 0.0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    constexpr double max_representable = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    if (a > max_representable)
    {
        return std::numeric_limits<double>::infinity();
    }
    const auto n = static_cast<std::uint32_t>(a);
    std::uint64_t result = 1;
    for (std::uint32_t ii = 1; ii <= n; ++ii)
    {
        if (result > std::numeric_limits<std::uint64_t>::max() / ii)
        {
            return std::numeric_limits<double>::infinity();
        }
        result *= ii;
    }
    return static_cast<double>(result);
}

/// @internal @brief Number of combinations of `r` items chosen from `n`.
double math_ncr(double n, double r) noexcept
{
    if (n < 0.0 || r < 0.0 || n < r)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    constexpr double max_representable = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    if (n > max_representable || r > max_representable)
    {
        return std::numeric_limits<double>::infinity();
    }
    const auto un = static_cast<std::uint64_t>(n);
    auto ur = static_cast<std::uint64_t>(r);
    if (ur > un - ur)
    {
        ur = un - ur;
    }
    std::uint64_t result = 1;
    for (std::uint64_t ii = 1; ii <= ur; ++ii)
    {
        if (result > std::numeric_limits<std::uint64_t>::max() / (un - ur + ii))
        {
            return std::numeric_limits<double>::infinity();
        }
        result *= (un - ur + ii);
        result /= ii;
    }
    return static_cast<double>(result);
}

/// @internal @brief Number of permutations of `r` items chosen from `n`.
double math_npr(double n, double r) noexcept
{
    return math_ncr(n, r) * math_fac(r);
}

/// @internal @brief One entry of the alphabetically sorted built-in function table.
struct BuiltinEntry
{
    std::string_view name;
    NativeFunction function;
    std::uint8_t arity;
    bool pure;
};

// clang-format off
/// @internal @brief The built-in functions, sorted by name to allow binary search.
constexpr std::array<BuiltinEntry, 35> k_builtins{{
    {"abs", &detail::invoke1<math_abs>, 1, true},
    {"acos", &detail::invoke1<math_acos>, 1, true},
    {"acosh", &detail::invoke1<math_acosh>, 1, true},
    {"asin", &detail::invoke1<math_asin>, 1, true},
    {"asinh", &detail::invoke1<math_asinh>, 1, true},
    {"atan", &detail::invoke1<math_atan>, 1, true},
    {"atan2", &detail::invoke2<math_atan2>, 2, true},
    {"atanh", &detail::invoke1<math_atanh>, 1, true},
    {"cbrt", &detail::invoke1<math_cbrt>, 1, true},
    {"ceil", &detail::invoke1<math_ceil>, 1, true},
    {"clamp", &detail::invoke3<math_clamp>, 3, true},
    {"cos", &detail::invoke1<math_cos>, 1, true},
    {"cosh", &detail::invoke1<math_cosh>, 1, true},
    {"e", &detail::invoke0<math_e>, 0, true},
    {"erf", &detail::invoke1<math_erf>, 1, true},
    {"erfc", &detail::invoke1<math_erfc>, 1, true},
    {"exp", &detail::invoke1<math_exp>, 1, true},
    {"exp2", &detail::invoke1<math_exp2>, 1, true},
    {"fac", &detail::invoke1<math_fac>, 1, true},
    {"floor", &detail::invoke1<math_floor>, 1, true},
    {"ln", &detail::invoke1<math_ln>, 1, true},
    {"log", &detail::invoke1<math_log10>, 1, true},
    {"log10", &detail::invoke1<math_log10>, 1, true},
    {"log2", &detail::invoke1<math_log2>, 1, true},
    {"max", &detail::invoke2<math_max>, 2, true},
    {"min", &detail::invoke2<math_min>, 2, true},
    {"ncr", &detail::invoke2<math_ncr>, 2, true},
    {"npr", &detail::invoke2<math_npr>, 2, true},
    {"pi", &detail::invoke0<math_pi>, 0, true},
    {"pow", &detail::invoke2<math_pow>, 2, true},
    {"sin", &detail::invoke1<math_sin>, 1, true},
    {"sinh", &detail::invoke1<math_sinh>, 1, true},
    {"sqrt", &detail::invoke1<math_sqrt>, 1, true},
    {"tan", &detail::invoke1<math_tan>, 1, true},
    {"tanh", &detail::invoke1<math_tanh>, 1, true},
}};
// clang-format on

static_assert(std::ranges::is_sorted(k_builtins, {}, &BuiltinEntry::name),
              "k_builtins must stay sorted by name for lower_bound to work");

/// @internal @brief Looks up a built-in function by name, or returns null.
const BuiltinEntry* find_builtin(std::string_view name) noexcept
{
    const auto it = std::ranges::lower_bound(k_builtins, name, {}, &BuiltinEntry::name);
    if (it != k_builtins.end() && it->name == name)
    {
        return &*it;
    }
    return nullptr;
}

// ---------------------------------------------------------------------
// Arithmetic operators. Each one is a plain NativeFunction so operator
// nodes need no special case in the evaluator.
// ---------------------------------------------------------------------

// clang-format off
double op_add(std::span<const double> a, void *) noexcept    { return a[0] + a[1]; }
double op_sub(std::span<const double> a, void *) noexcept    { return a[0] - a[1]; }
double op_mul(std::span<const double> a, void *) noexcept    { return a[0] * a[1]; }
double op_div(std::span<const double> a, void *) noexcept    { return a[0] / a[1]; }
double op_mod(std::span<const double> a, void *) noexcept    { return std::fmod(a[0], a[1]); }
double op_pow(std::span<const double> a, void *) noexcept    { return std::pow(a[0], a[1]); }
double op_negate(std::span<const double> a, void *) noexcept { return -a[0]; }
double op_comma(std::span<const double> a, void *) noexcept  { return a[1]; }
// clang-format on

// ---------------------------------------------------------------------
// Lexer.
// ---------------------------------------------------------------------

/// @internal @brief The kind of token most recently read by the @ref Lexer.
enum class TokenType : std::uint8_t
{
    End,     ///< The input is exhausted.
    Error,   ///< The input does not match any valid token (bad character or malformed number).
    Unknown, ///< A well-formed identifier that names no known variable or function.
    Number,  ///< A numeric literal.
    Ident,   ///< A bound variable.
    Call,    ///< A native function name.
    Open,    ///< '('.
    Close,   ///< ')'.
    Sep,     ///< ','.
    Infix,   ///< A binary or unary-capable operator.
};

/// @internal @brief Which operator an @ref TokenType::Infix token spells.
enum class InfixOp : std::uint8_t
{
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow
};

/// @internal @brief The decoded contents of one lexical token.
struct Token
{
    TokenType type = TokenType::End;
    double number = 0.0;
    const double* bound = nullptr;
    NativeFunction function = nullptr;
    void* context = nullptr;
    std::uint8_t arity = 0;
    bool pure = false;
    InfixOp infix = InfixOp::Add;
};

/// @internal @brief Returns true if `c` may start or continue an identifier.
bool is_ident_char(char c) noexcept
{
    const auto uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) != 0 || c == '_';
}

/// @internal @brief Returns true if `c` may start an identifier.
bool is_ident_start(char c) noexcept
{
    const auto uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) != 0;
}

} // namespace

/// @internal @brief Turns an expression string into a stream of @ref Token values.
class Lexer
{
public:
    Lexer(std::string_view expression, std::span<const Variable> variables) noexcept
        : cursor_(expression.data()), end_(expression.data() + expression.size()), start_(expression.data()),
          token_start_(expression.data()), variables_(variables)
    {
    }

    /// @brief Reads the next token into @ref current.
    void advance() noexcept
    {
        while (cursor_ != end_ && is_space(*cursor_))
        {
            ++cursor_;
        }
        token_start_ = cursor_;
        if (cursor_ == end_)
        {
            current_ = Token{};
            current_.type = TokenType::End;
            return;
        }
        const char c = *cursor_;
        if ((c >= '0' && c <= '9') || c == '.')
        {
            read_number();
        }
        else if (is_ident_start(c))
        {
            read_identifier();
        }
        else
        {
            read_operator();
        }
    }

    /// @brief The most recently read token.
    const Token& current() const noexcept
    {
        return current_;
    }

    /// @brief The offset of the current token's first character.
    std::size_t position() const noexcept
    {
        return static_cast<std::size_t>(token_start_ - start_);
    }

private:
    static bool is_space(char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    void read_number() noexcept
    {
        current_ = Token{};
        if (cursor_ + 1 < end_ && cursor_[0] == '0' && (cursor_[1] == 'x' || cursor_[1] == 'X'))
        {
            const char* digits_start = cursor_ + 2;
            unsigned long long hex_value = 0;
            const auto hex_result = std::from_chars(digits_start, end_, hex_value, 16);
            if (hex_result.ec == std::errc{} && hex_result.ptr != digits_start)
            {
                current_.number = static_cast<double>(hex_value);
                current_.type = TokenType::Number;
                cursor_ = hex_result.ptr;
                return;
            }
        }
        double value = 0.0;
        const auto result = std::from_chars(cursor_, end_, value);
        if (result.ec == std::errc{})
        {
            current_.number = value;
            current_.type = TokenType::Number;
            cursor_ = result.ptr;
        }
        else
        {
            current_.type = TokenType::Error;
            ++cursor_;
        }
    }

    void read_identifier() noexcept
    {
        current_ = Token{};
        const char* name_start = cursor_;
        while (cursor_ != end_ && is_ident_char(*cursor_))
        {
            ++cursor_;
        }
        const std::string_view name(name_start, static_cast<std::size_t>(cursor_ - name_start));

        if (const Variable* var = find_variable(name); var != nullptr)
        {
            if (var->bound_ != nullptr)
            {
                current_.type = TokenType::Ident;
                current_.bound = var->bound_;
            }
            else
            {
                current_.type = TokenType::Call;
                current_.function = var->function_;
                current_.context = var->context_;
                current_.arity = var->arity_;
                current_.pure = var->pure_;
            }
            return;
        }
        if (const BuiltinEntry* fn = find_builtin(name); fn != nullptr)
        {
            current_.type = TokenType::Call;
            current_.function = fn->function;
            current_.context = nullptr;
            current_.arity = fn->arity;
            current_.pure = fn->pure;
            return;
        }
        current_.type = TokenType::Unknown;
    }

    void read_operator() noexcept
    {
        current_ = Token{};
        const char c = *cursor_;
        ++cursor_;
        switch (c)
        {
        case '+':
            current_.type = TokenType::Infix;
            current_.infix = InfixOp::Add;
            break;
        case '-':
            current_.type = TokenType::Infix;
            current_.infix = InfixOp::Sub;
            break;
        case '*':
            current_.type = TokenType::Infix;
            current_.infix = InfixOp::Mul;
            break;
        case '/':
            current_.type = TokenType::Infix;
            current_.infix = InfixOp::Div;
            break;
        case '^':
            current_.type = TokenType::Infix;
            current_.infix = InfixOp::Pow;
            break;
        case '%':
            current_.type = TokenType::Infix;
            current_.infix = InfixOp::Mod;
            break;
        case '(':
            current_.type = TokenType::Open;
            break;
        case ')':
            current_.type = TokenType::Close;
            break;
        case ',':
            current_.type = TokenType::Sep;
            break;
        default:
            current_.type = TokenType::Error;
            break;
        }
    }

    const Variable* find_variable(std::string_view name) const noexcept
    {
        for (const Variable& var : variables_)
        {
            if (var.name_ == name)
            {
                return &var;
            }
        }
        return nullptr;
    }

    const char* cursor_;
    const char* end_;
    const char* start_;
    const char* token_start_;
    std::span<const Variable> variables_;
    Token current_{};
};

// ---------------------------------------------------------------------
// Parser.
// ---------------------------------------------------------------------

/// @internal @brief A recursive descent parser that builds a syntax tree from a @ref Lexer.
class Parser
{
public:
    Parser(std::string_view expression, std::span<const Variable> variables) noexcept : lexer_(expression, variables)
    {
        lexer_.advance();
    }

    /// @brief Parses the whole expression, requiring every character to be consumed.
    std::expected<std::unique_ptr<Node>, ParseError> parse()
    {
        std::unique_ptr<Node> root = parse_list();
        if (!root)
        {
            return std::unexpected(std::move(*error_));
        }
        if (lexer_.current().type != TokenType::End)
        {
            fail(ErrorKind::TrailingInput, "unexpected extra input after the expression");
            return std::unexpected(std::move(*error_));
        }
        return root;
    }

private:
    using NodePtr = std::unique_ptr<Node>;

    NodePtr parse_list()
    {
        NodePtr ret = parse_expr();
        if (!ret)
        {
            return nullptr;
        }
        while (lexer_.current().type == TokenType::Sep)
        {
            lexer_.advance();
            NodePtr rhs = parse_expr();
            if (!rhs)
            {
                return nullptr;
            }
            NodePtr node = make_call(&op_comma, nullptr, true);
            node->children.push_back(std::move(ret));
            node->children.push_back(std::move(rhs));
            ret = std::move(node);
        }
        return ret;
    }

    NodePtr parse_expr()
    {
        NodePtr ret = parse_term();
        if (!ret)
        {
            return nullptr;
        }
        while (lexer_.current().type == TokenType::Infix &&
               (lexer_.current().infix == InfixOp::Add || lexer_.current().infix == InfixOp::Sub))
        {
            const InfixOp op = lexer_.current().infix;
            lexer_.advance();
            NodePtr rhs = parse_term();
            if (!rhs)
            {
                return nullptr;
            }
            ret = make_binary(op, std::move(ret), std::move(rhs));
        }
        return ret;
    }

    NodePtr parse_term()
    {
        NodePtr ret = parse_factor();
        if (!ret)
        {
            return nullptr;
        }
        while (lexer_.current().type == TokenType::Infix &&
               (lexer_.current().infix == InfixOp::Mul || lexer_.current().infix == InfixOp::Div ||
                lexer_.current().infix == InfixOp::Mod))
        {
            const InfixOp op = lexer_.current().infix;
            lexer_.advance();
            NodePtr rhs = parse_factor();
            if (!rhs)
            {
                return nullptr;
            }
            ret = make_binary(op, std::move(ret), std::move(rhs));
        }
        return ret;
    }

    /*
        factor is now just an alias for unary: '*', '/', '%' bind to whatever
        a signed power expression produces.
    */
    NodePtr parse_factor()
    {
        return parse_unary();
    }

    /*
        unary consumes any leading '+'/'-' signs and applies them *after*
        parsing a full (possibly chained) power expression, so '^' binds
        tighter than a leading unary minus: -2^2 parses as -(2^2) rather
        than (-2)^2.
    */
    NodePtr parse_unary()
    {
        int sign = 1;
        while (lexer_.current().type == TokenType::Infix &&
               (lexer_.current().infix == InfixOp::Add || lexer_.current().infix == InfixOp::Sub))
        {
            if (lexer_.current().infix == InfixOp::Sub)
            {
                sign = -sign;
            }
            lexer_.advance();
        }
        NodePtr power_node = parse_power();
        if (!power_node)
        {
            return nullptr;
        }
        if (sign == 1)
        {
            return power_node;
        }
        NodePtr negated = make_call(&op_negate, nullptr, true);
        negated->children.push_back(std::move(power_node));
        return negated;
    }

    /*
        power -> base ('^' unary)?, right-associative: 2^3^2 is 2^(3^2), not
        (2^3)^2. The exponent recurses into parse_unary (rather than back
        into parse_power) so a sign on the exponent, as in 2^-2, is still
        accepted without parentheses, applying only to that operand -- it
        can never climb back up and bind the base of the whole chain, since
        that sign is consumed one level up, in parse_unary, before this
        function is ever called.
    */
    NodePtr parse_power()
    {
        NodePtr ret = parse_base();
        if (!ret)
        {
            return nullptr;
        }
        if (lexer_.current().type == TokenType::Infix && lexer_.current().infix == InfixOp::Pow)
        {
            lexer_.advance();
            NodePtr rhs = parse_unary();
            if (!rhs)
            {
                return nullptr;
            }
            ret = make_binary(InfixOp::Pow, std::move(ret), std::move(rhs));
        }
        return ret;
    }

    NodePtr parse_base()
    {
        const Token tok = lexer_.current();
        switch (tok.type)
        {
        case TokenType::Number: {
            lexer_.advance();
            return make_constant(tok.number);
        }
        case TokenType::Ident: {
            lexer_.advance();
            return make_variable(tok.bound);
        }
        case TokenType::Call:
            return parse_call(tok);
        case TokenType::Open: {
            lexer_.advance();
            NodePtr node = parse_list();
            if (!node)
            {
                return nullptr;
            }
            if (lexer_.current().type != TokenType::Close)
            {
                fail(ErrorKind::UnbalancedParentheses, "expected a closing parenthesis");
                return nullptr;
            }
            lexer_.advance();
            return node;
        }
        case TokenType::Unknown:
            fail(ErrorKind::UnknownIdentifier, "identifier matches no known variable or function");
            return nullptr;
        case TokenType::Error:
            fail(ErrorKind::UnexpectedToken, "found an unrecognized character or malformed token");
            return nullptr;
        default:
            fail(ErrorKind::UnexpectedToken, "expected a number, a variable, a function call, or '('");
            return nullptr;
        }
    }

    NodePtr parse_call(const Token& tok)
    {
        if (static_cast<std::size_t>(tok.arity) > k_max_arity)
        {
            fail(ErrorKind::ArityTooLarge, "a function was registered with too many arguments");
            return nullptr;
        }
        NodePtr node = make_call(tok.function, tok.context, tok.pure);
        lexer_.advance();

        if (tok.arity == 0)
        {
            if (lexer_.current().type == TokenType::Open)
            {
                lexer_.advance();
                if (lexer_.current().type != TokenType::Close)
                {
                    fail(ErrorKind::UnbalancedParentheses, "expected a closing parenthesis after '('");
                    return nullptr;
                }
                lexer_.advance();
            }
            return node;
        }

        if (tok.arity == 1)
        {
            /*
                A one-argument function binds as tightly as unary minus does,
                so it can take its argument without surrounding parentheses,
                e.g. "sin -2" and "sin 2^2" both work without parens.
            */
            NodePtr arg = parse_unary();
            if (!arg)
            {
                return nullptr;
            }
            node->children.push_back(std::move(arg));
            return node;
        }

        if (lexer_.current().type != TokenType::Open)
        {
            fail(ErrorKind::MissingArgument, "expected '(' to start the argument list");
            return nullptr;
        }
        std::uint8_t ii = 0;
        for (; ii < tok.arity; ++ii)
        {
            lexer_.advance();
            NodePtr arg = parse_expr();
            if (!arg)
            {
                return nullptr;
            }
            node->children.push_back(std::move(arg));
            if (lexer_.current().type != TokenType::Sep)
            {
                break;
            }
        }
        if (lexer_.current().type != TokenType::Close || ii != tok.arity - 1)
        {
            fail(ErrorKind::MissingArgument, "the function call has the wrong number of arguments");
            return nullptr;
        }
        lexer_.advance();
        return node;
    }

    static NodePtr make_constant(double value)
    {
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::Constant;
        node->value = value;
        return node;
    }

    static NodePtr make_variable(const double* bound)
    {
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::Variable;
        node->bound = bound;
        return node;
    }

    static NodePtr make_call(NativeFunction fn, void* context, bool pure)
    {
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::Call;
        node->function = fn;
        node->context = context;
        node->pure = pure;
        return node;
    }

    static NodePtr make_binary(InfixOp op, NodePtr lhs, NodePtr rhs)
    {
        NativeFunction fn = &op_add;
        switch (op)
        {
        case InfixOp::Add:
            fn = &op_add;
            break;
        case InfixOp::Sub:
            fn = &op_sub;
            break;
        case InfixOp::Mul:
            fn = &op_mul;
            break;
        case InfixOp::Div:
            fn = &op_div;
            break;
        case InfixOp::Mod:
            fn = &op_mod;
            break;
        case InfixOp::Pow:
            fn = &op_pow;
            break;
        }
        NodePtr node = make_call(fn, nullptr, true);
        node->children.push_back(std::move(lhs));
        node->children.push_back(std::move(rhs));
        return node;
    }

    void fail(ErrorKind kind, std::string message) noexcept
    {
        if (!error_)
        {
            error_ = ParseError{lexer_.position(), kind, std::move(message)};
        }
    }

    Lexer lexer_;
    std::optional<ParseError> error_;
};

// ---------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------

Variable Variable::bind(std::string_view name, const double& value) noexcept
{
    Variable v;
    v.name_ = name;
    v.bound_ = &value;
    return v;
}

Variable Variable::closure(std::string_view name, NativeFunction fn, std::uint8_t arity, void* context,
                           bool pure) noexcept
{
    Variable v;
    v.name_ = name;
    v.function_ = fn;
    v.context_ = context;
    v.arity_ = arity;
    v.pure_ = pure;
    return v;
}

Expression::Expression(std::unique_ptr<detail::Node> root) noexcept : root_(std::move(root))
{
}
Expression::Expression(Expression&& other) noexcept = default;
Expression& Expression::operator=(Expression&& other) noexcept = default;
Expression::~Expression() = default;

double Expression::eval() const noexcept
{
    return root_ ? evaluate(*root_) : std::numeric_limits<double>::quiet_NaN();
}

std::string Expression::to_string() const
{
    std::string out;
    if (root_)
    {
        print_node(*root_, 0, out);
    }
    return out;
}

std::expected<Expression, ParseError> compile(std::string_view expression, std::span<const Variable> variables)
{
    if (expression.empty())
    {
        return std::unexpected(ParseError{0, ErrorKind::EmptyExpression, "the expression is empty"});
    }
    Parser parser(expression, variables);
    std::expected<std::unique_ptr<detail::Node>, ParseError> result = parser.parse();
    if (!result)
    {
        return std::unexpected(std::move(result.error()));
    }
    optimize(*result.value());
    return Expression(std::move(result.value()));
}

std::expected<double, ParseError> interpret(std::string_view expression, std::span<const Variable> variables)
{
    std::expected<Expression, ParseError> compiled = compile(expression, variables);
    if (!compiled)
    {
        return std::unexpected(std::move(compiled.error()));
    }
    return compiled->eval();
}

} // namespace kb::expr