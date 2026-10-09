#include "kibble/string/parse.h"

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace kb::su
{
namespace
{

// ---------------------------------------------------------------------------
// Slow path: exact decimal arithmetic.
// ---------------------------------------------------------------------------

constexpr std::uint32_t k_max_digits = 800; ///< Digits kept. 767 digits are enough to round any double.

/// @internal @brief Decimal number with a fixed digit capacity.
struct Decimal
{
    // Not initialized on purpose.
    std::uint8_t digits[k_max_digits]; ///< Digits 0..9, most significant first.
    std::uint32_t count = 0;           ///< Number of digits in use.
    std::int32_t point = 0;            ///< Value is 0.d0d1d2... * 10^point.
    bool truncated = false;            ///< True if a non-zero digit was dropped.
};

/// @internal @brief Remove trailing zero digits.
void trim(Decimal& a)
{
    while (a.count > 0 && a.digits[a.count - 1] == 0)
    {
        --a.count;
    }
    if (a.count == 0)
    {
        a.point = 0;
    }
}

/// @internal @brief Multiply the decimal by 2^k. The value k must be 60 or less.
void left_shift(Decimal& a, std::uint32_t k)
{
    std::uint8_t reversed[k_max_digits + 24]; // Least significant digit first.
    std::uint32_t produced = 0;
    std::uint64_t carry = 0;
    for (std::uint32_t ii = a.count; ii > 0; --ii)
    {
        carry += static_cast<std::uint64_t>(a.digits[ii - 1]) << k;
        const std::uint64_t quotient = carry / 10;
        reversed[produced++] = static_cast<std::uint8_t>(carry - 10 * quotient);
        carry = quotient;
    }
    while (carry > 0)
    {
        const std::uint64_t quotient = carry / 10;
        reversed[produced++] = static_cast<std::uint8_t>(carry - 10 * quotient);
        carry = quotient;
    }

    const std::uint32_t old_count = a.count;
    const std::uint32_t keep = produced < k_max_digits ? produced : k_max_digits;
    for (std::uint32_t ii = 0; ii < keep; ++ii)
    {
        a.digits[ii] = reversed[produced - 1 - ii];
    }
    for (std::uint32_t ii = 0; ii < produced - keep; ++ii)
    {
        if (reversed[ii] != 0)
        {
            a.truncated = true;
        }
    }
    a.count = keep;
    a.point += static_cast<std::int32_t>(produced) - static_cast<std::int32_t>(old_count);
    trim(a);
}

/// @internal @brief Divide the decimal by 2^k. The value k must be 60 or less.
void right_shift(Decimal& a, std::uint32_t k)
{
    std::uint32_t rd = 0; // Read index.
    std::uint32_t wr = 0; // Write index.
    std::uint64_t n = 0;

    // Read enough leading digits to make the first output digit.
    for (; (n >> k) == 0; ++rd)
    {
        if (rd >= a.count)
        {
            if (n == 0)
            {
                a.count = 0;
                return;
            }
            while ((n >> k) == 0)
            {
                n *= 10;
                ++rd;
            }
            break;
        }
        n = n * 10 + a.digits[rd];
    }
    a.point -= static_cast<std::int32_t>(rd) - 1;

    const std::uint64_t mask = (std::uint64_t{1} << k) - 1;
    for (; rd < a.count; ++rd)
    {
        const std::uint8_t digit_in = a.digits[rd];
        a.digits[wr++] = static_cast<std::uint8_t>(n >> k);
        n = (n & mask) * 10 + digit_in;
    }

    // Write the digits of the remainder. Digits that do not fit set the truncated flag.
    while (n > 0)
    {
        const std::uint8_t digit_out = static_cast<std::uint8_t>(n >> k);
        n &= mask;
        if (wr < k_max_digits)
        {
            a.digits[wr++] = digit_out;
        }
        else if (digit_out != 0)
        {
            a.truncated = true;
        }
        n *= 10;
    }
    a.count = wr;
    trim(a);
}

/// @internal @brief Multiply the decimal by 2^k if k is positive. Divide it by 2^-k if k is negative.
void shift(Decimal& a, std::int32_t k)
{
    constexpr std::int32_t k_max_shift = 60;
    if (a.count == 0)
    {
        return;
    }
    while (k > k_max_shift)
    {
        left_shift(a, static_cast<std::uint32_t>(k_max_shift));
        k -= k_max_shift;
    }
    while (k < -k_max_shift)
    {
        right_shift(a, static_cast<std::uint32_t>(k_max_shift));
        k += k_max_shift;
    }
    if (k > 0)
    {
        left_shift(a, static_cast<std::uint32_t>(k));
    }
    else if (k < 0)
    {
        right_shift(a, static_cast<std::uint32_t>(-k));
    }
}

/// @internal @brief Check if rounding at digit position `at` must round up.
bool should_round_up(const Decimal& a, std::int32_t at)
{
    if (at < 0 || static_cast<std::uint32_t>(at) >= a.count)
    {
        return false;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(at);
    if (a.digits[index] == 5 && index + 1 == a.count)
    {
        // Exactly half: round to even. Dropped digits mean the value is above half.
        if (a.truncated)
        {
            return true;
        }
        return index > 0 && (a.digits[index - 1] & 1) != 0;
    }
    return a.digits[index] >= 5;
}

/// @internal @brief Convert the decimal to an integer. Rounds to nearest, ties to even.
std::uint64_t rounded_integer(const Decimal& a)
{
    if (a.point > 20)
    {
        return ~std::uint64_t{0};
    }
    const std::uint32_t integer_count = a.point > 0 ? static_cast<std::uint32_t>(a.point) : 0U;
    std::uint32_t ii = 0;
    std::uint64_t n = 0;
    for (; ii < integer_count && ii < a.count; ++ii)
    {
        n = n * 10 + a.digits[ii];
    }
    for (; ii < integer_count; ++ii)
    {
        n *= 10;
    }
    if (should_round_up(a, a.point))
    {
        ++n;
    }
    return n;
}

/// @internal @brief Convert the decimal to the nearest double. Changes the decimal. Returns false on overflow.
bool decimal_to_double(Decimal& d, double& out)
{
    constexpr std::uint32_t k_mantissa_bits = 52;
    constexpr std::int32_t k_exp_bias = 1023;
    constexpr std::int32_t k_max_biased_exp = 2047;

    if (d.count == 0)
    {
        out = 0.0;
        return true;
    }
    if (d.point > 310)
    {
        return false;
    }
    if (d.point < -330)
    {
        out = 0.0;
        return true;
    }

    // Scale by powers of two until the value is in [0.5, 1).
    static constexpr std::int32_t k_pow_tab[] = {1, 3, 6, 9, 13, 16, 19, 23, 26};
    std::int32_t exp2 = 0;
    while (d.point > 0)
    {
        const std::int32_t n = d.point >= 9 ? 27 : k_pow_tab[d.point];
        shift(d, -n);
        exp2 += n;
    }
    while (d.point < 0 || (d.point == 0 && d.digits[0] < 5))
    {
        const std::int32_t n = -d.point >= 9 ? 27 : k_pow_tab[-d.point];
        shift(d, n);
        exp2 -= n;
    }

    --exp2; // The range changes from [0.5, 1) to [1, 2).

    if (exp2 < -k_exp_bias + 1) // Subnormal range: shift the value right to the minimum exponent.
    {
        const std::int32_t n = -k_exp_bias + 1 - exp2;
        shift(d, -n);
        exp2 += n;
    }
    if (exp2 + k_exp_bias >= k_max_biased_exp)
    {
        return false;
    }

    shift(d, static_cast<std::int32_t>(k_mantissa_bits + 1));
    std::uint64_t mant = rounded_integer(d);
    if (mant == (std::uint64_t{2} << k_mantissa_bits)) // Rounding carried into a new bit.
    {
        mant >>= 1;
        ++exp2;
        if (exp2 + k_exp_bias >= k_max_biased_exp)
        {
            return false;
        }
    }
    if ((mant & (std::uint64_t{1} << k_mantissa_bits)) == 0) // Subnormal: no implicit leading bit.
    {
        exp2 = -k_exp_bias;
    }

    std::uint64_t bits = mant & ((std::uint64_t{1} << k_mantissa_bits) - 1);
    bits |= static_cast<std::uint64_t>(exp2 + k_exp_bias) << k_mantissa_bits;
    std::memcpy(&out, &bits, sizeof(out));
    return true;
}

constexpr std::int64_t k_point_limit = 100000; ///< A decimal point outside this range is out of double range.

/// @internal @brief Read the mantissa text [begin, end) into a decimal. The value `explicit_exp` is the number after
/// 'e'.
void build_decimal(const char* begin, const char* end, std::int64_t explicit_exp, Decimal& dec)
{
    std::int64_t point = 0;
    bool seen_dot = false;
    for (const char* pp = begin; pp != end; ++pp)
    {
        if (*pp == '.')
        {
            seen_dot = true;
            continue;
        }
        const std::uint8_t digit = static_cast<std::uint8_t>(*pp - '0');
        if (dec.count == 0 && digit == 0) // Leading zero.
        {
            if (seen_dot)
            {
                --point;
            }
            continue;
        }
        if (!seen_dot)
        {
            ++point;
        }
        if (dec.count < k_max_digits)
        {
            dec.digits[dec.count++] = digit;
        }
        else if (digit != 0)
        {
            dec.truncated = true;
        }
    }
    point += explicit_exp;
    if (point > k_point_limit)
    {
        point = k_point_limit;
    }
    if (point < -k_point_limit)
    {
        point = -k_point_limit;
    }
    dec.point = static_cast<std::int32_t>(point);
    trim(dec);
}

// ---------------------------------------------------------------------------
// Fast path: one exact multiply or divide. The operands are exact doubles and
// the IEEE operation rounds only once.
// ---------------------------------------------------------------------------

// The fast path needs real double arithmetic (no x87 double rounding).
#if defined(FLT_EVAL_METHOD) && (FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 1)
constexpr bool k_fast_path_safe = true; ///< True if the fast path is exact on this platform.
#else
constexpr bool k_fast_path_safe = false; ///< True if the fast path is exact on this platform.
#endif

constexpr double k_pow10_double[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22}; ///< 10^0 to 10^22 (exact).

constexpr std::uint64_t k_pow10_int[] = {1ull,
                                         10ull,
                                         100ull,
                                         1000ull,
                                         10000ull,
                                         100000ull,
                                         1000000ull,
                                         10000000ull,
                                         100000000ull,
                                         1000000000ull,
                                         10000000000ull,
                                         100000000000ull,
                                         1000000000000ull,
                                         10000000000000ull,
                                         100000000000000ull,
                                         1000000000000000ull}; ///< 10^0 to 10^15.

/// @internal @brief Convert with one exact multiply or divide. Returns false if the result can be inexact.
bool try_fast_path(std::uint64_t mantissa, std::int64_t exp10, double& value)
{
    if (!k_fast_path_safe || exp10 < -22 || exp10 > 22 + 15)
    {
        return false;
    }
    if (exp10 > 22) // Move extra powers of ten into the mantissa while it stays <= 2^53.
    {
        const std::int64_t extra = exp10 - 22;
        if (mantissa > (std::uint64_t{1} << 53) / k_pow10_int[extra])
        {
            return false;
        }
        mantissa *= k_pow10_int[extra];
        exp10 = 22;
    }
    if (mantissa > (std::uint64_t{1} << 53))
    {
        return false;
    }
    value = static_cast<double>(mantissa);
    if (exp10 < 0)
    {
        value /= k_pow10_double[-exp10];
    }
    else
    {
        value *= k_pow10_double[exp10];
    }
    return true;
}

/// @internal @brief Check if a character is a decimal digit.
inline bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

} // namespace

std::string_view trim_sv(std::string_view str)
{
    const std::size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }

    const std::size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool parse_double(std::string_view token, double& result)
{
    const char* pp = token.data();
    const char* const end = pp + token.size();
    if (pp == end)
    {
        return false;
    }

    bool negative = false;
    if (*pp == '-')
    {
        negative = true;
        ++pp;
    }
    else if (*pp == '+')
    {
        ++pp;
    }

    const char* const mantissa_begin = pp;
    std::uint64_t mantissa = 0; // Up to 19 significant digits.
    std::uint32_t significant = 0;
    std::int64_t exp10 = 0;
    bool any_digits = false;
    bool dropped_nonzero = false;

    for (; pp != end && is_digit(*pp); ++pp)
    {
        any_digits = true;
        const std::uint32_t digit = static_cast<std::uint32_t>(*pp - '0');
        if (mantissa == 0 && digit == 0)
        {
            continue;
        }
        if (significant < 19)
        {
            mantissa = mantissa * 10 + digit;
            ++significant;
        }
        else
        {
            ++exp10; // A dropped integer digit still scales the value.
            dropped_nonzero |= digit != 0;
        }
    }
    if (pp != end && *pp == '.')
    {
        ++pp;
        for (; pp != end && is_digit(*pp); ++pp)
        {
            any_digits = true;
            const std::uint32_t digit = static_cast<std::uint32_t>(*pp - '0');
            if (mantissa == 0 && digit == 0)
            {
                --exp10;
                continue;
            }
            if (significant < 19)
            {
                mantissa = mantissa * 10 + digit;
                ++significant;
                --exp10;
            }
            else
            {
                dropped_nonzero |= digit != 0;
            }
        }
    }
    if (!any_digits)
    {
        return false;
    }
    const char* const mantissa_end = pp;

    std::int64_t explicit_exp = 0;
    if (pp != end && (*pp == 'e' || *pp == 'E'))
    {
        ++pp;
        bool exp_negative = false;
        if (pp != end && (*pp == '-' || *pp == '+'))
        {
            exp_negative = *pp == '-';
            ++pp;
        }
        if (pp == end || !is_digit(*pp))
        {
            return false;
        }
        for (; pp != end && is_digit(*pp); ++pp)
        {
            if (explicit_exp < 100000000) // Saturate. A larger exponent is out of range anyway.
            {
                explicit_exp = explicit_exp * 10 + (*pp - '0');
            }
        }
        if (exp_negative)
        {
            explicit_exp = -explicit_exp;
        }
    }
    if (pp != end)
    {
        return false;
    }

    if (mantissa == 0) // All digits are zero.
    {
        result = negative ? -0.0 : 0.0;
        return true;
    }

    double value = 0.0;
    if (!dropped_nonzero && try_fast_path(mantissa, exp10 + explicit_exp, value))
    {
        result = negative ? -value : value;
        return true;
    }

    Decimal dec;
    build_decimal(mantissa_begin, mantissa_end, explicit_exp, dec);
    if (!decimal_to_double(dec, value))
    {
        return false; // Overflow.
    }
    result = negative ? -value : value;
    return true;
}

} // namespace kb::su