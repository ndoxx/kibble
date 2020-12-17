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
float constexpr fsqrt_newton_raphson(float x, float curr, float prev)
{
    return curr == prev ? curr : fsqrt_newton_raphson(x, 0.5f * (curr + x / curr), curr);
}
double constexpr sqrt_newton_raphson(double x, double curr, double prev)
{
    return curr == prev ? curr : sqrt_newton_raphson(x, 0.5 * (curr + x / curr), curr);
}
} // namespace detail

/*
 * Constexpr version of the square root
 * Return value:
 *   - For a finite and non-negative value of "x", returns an approximation for the square root of "x"
 *   - Otherwise, returns NaN
 * Original Author (I suppose): Alex Shtof
 *   - https://stackoverflow.com/questions/8622256/in-c11-is-sqrt-defined-as-constexpr
 */
float constexpr fsqrt(float x)
{
    return x >= 0 && x < std::numeric_limits<float>::infinity() ? detail::fsqrt_newton_raphson(x, x, 0)
                                                                : std::numeric_limits<float>::quiet_NaN();
}
double constexpr sqrt(double x)
{
    return x >= 0 && x < std::numeric_limits<double>::infinity() ? detail::sqrt_newton_raphson(x, x, 0)
                                                                 : std::numeric_limits<double>::quiet_NaN();
}

// Previous power of 2 of x
constexpr uint32_t pp2(uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

// Next power of 2 of x
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

} // namespace math
} // namespace kb