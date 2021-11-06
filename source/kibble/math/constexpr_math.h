#pragma once
#include <cstdint>
#include <limits>

/*
    Collection of various compile-time math functions.
*/

namespace kb
{
namespace math
{
namespace detail
{
// Recursive Newton-Raphson implementations to approximate a square root.
// Stops when the floating point number is stable.
float constexpr fsqrt_newton_raphson(float x, float curr, float prev)
{
    return curr == prev ? curr : fsqrt_newton_raphson(x, 0.5f * (curr + x / curr), curr);
}
double constexpr sqrt_newton_raphson(double x, double curr, double prev)
{
    return curr == prev ? curr : sqrt_newton_raphson(x, 0.5 * (curr + x / curr), curr);
}
} // namespace detail

/**
 * @brief Constexpr version of the square root.
 * Original Author (I suppose): Alex Shtof
 * see: https://stackoverflow.com/questions/8622256/in-c11-is-sqrt-defined-as-constexpr
 *
 * @param x
 * @return For a finite and non-negative value of "x", returns an approximation for the square root of "x", otherwise,
 * returns NaN
 */
float constexpr fsqrt(float x)
{
    return x >= 0 && x < std::numeric_limits<float>::infinity() ? detail::fsqrt_newton_raphson(x, x, 0)
                                                                : std::numeric_limits<float>::quiet_NaN();
}

/**
 * @brief Constexpr version of the square root.
 * For double values.
 *
 * @param x
 * @return double constexpr
 * @see fsqrt()
 */
double constexpr sqrt(double x)
{
    return x >= 0 && x < std::numeric_limits<double>::infinity() ? detail::sqrt_newton_raphson(x, x, 0)
                                                                 : std::numeric_limits<double>::quiet_NaN();
}

/**
 * @brief Previous power of 2 of x
 * May originate from the Hacker's Delight book
 *
 * @param x
 * @return constexpr uint32_t
 */
constexpr uint32_t pp2(uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

/**
 * @brief Next power of 2 of x
 * May originate from the Hacker's Delight book
 *
 * @param x
 * @return constexpr uint32_t
 */
constexpr uint32_t np2(uint32_t x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

/**
 * @brief Compute the parity of the argument (-1)^ii.
 *
 * @param ii
 * @return constexpr int
 */
constexpr int parity(int ii)
{
    return ((ii % 2) ? -1 : 1);
}

/**
 * @brief Compute n! recursively.
 *
 * @param n
 * @return constexpr int
 */
constexpr int factorial(int n)
{
    return n <= 1 ? 1 : (n * factorial(n - 1));
}

/**
 * @brief Compute the binomial coefficient n choose i
 *
 * @param nn Number of things to choose from
 * @param ii Number of things to choose
 * @return constexpr int
 */
constexpr int choose(int nn, int ii)
{
    return (ii <= nn) ? factorial(nn) / (factorial(ii) * factorial(nn - ii)) : 0;
}

} // namespace math
} // namespace kb