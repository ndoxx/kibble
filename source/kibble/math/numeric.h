#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <utility>

namespace kb
{
namespace math
{

/**
 * @brief Implementation of the Newton-Raphson algorithm.
 * This algorithm is a means to compute a root of a function `f`, given an initial guess `xx` and its derivative. If the
 * initial guess is close enough to the root, the algorithm will quickly converge. The algorithm is iterated until the
 * error with respect to the root of the function f (value of f_over_fprime) becomes less than epsilon or the maximum
 * number of iterations has been reached.
 *
 * @param f_over_fprime Closed-form expression of f divided by its first derivative
 * @param xx Initial guess for the root. Must be close enough or anything can happen. What "enough" means is in fact a
 * really non trivial subject involving fractals...
 * @param epsilon Maximum allowable error for the root
 * @param max_iter Maximum number of iterations
 * @return std::pair<float, float> root approximation and error
 */
std::pair<float, float> newton_raphson(std::function<float(float)> f_over_fprime, float xx, float epsilon,
                                       size_t max_iter);

/**
 * @brief Helper method to find the initial guess of Newton-Raphson.
 * Advances along the curve using bigger and bigger steps until the function changes sign. The step size is in a
 * geometric progression controlled by the parameter alpha. The value returned is between the two values of x where the
 * sign change occurred.
 *
 * @param f The function to examine.
 * @param start_x Starting value for x
 * @param start_step Starting value for the step size
 * @param alpha Common ratio of the step size geometric progression
 * @return float A very crude approximation of the root, but a usable initial guess for the Newton-Raphson algorithm
 */
float nr_initial_guess_iterative(std::function<float(float)> f, float start_x, float start_step, float alpha = 2.f);

/**
 * @brief Integrate a function f between lb and ub using a specified number of subdivisions.
 * This is a composite Simpson's rule implementation. The idea is to subdivide the interval of integration into multiple
 * sub-intervals, small enough that the function is expected to be tame in each of them (quasi-linear). Then applying
 * the Simpson's rule in each interval and summing up all contributions is guaranteed to give a more accurate result.
 *
 * @param f The function to integrate
 * @param lb Lower bound of the integration interval
 * @param ub Upper bound of the integration interval
 * @param subdivisions Number of subdivisions
 * @return float
 */
float integrate_simpson(std::function<float(float)> f, float lb, float ub, uint32_t subdivisions);

/**
 * @brief Performs exponential moving average thanks to an IIR
 *
 * @tparam FloatT
 * @param accumulator filtered value
 * @param new_value new value to push
 * @param alpha damping coeff < 1.f (the higher, the less damped)
 */
template <typename FloatT = float>
    requires std::floating_point<FloatT>
inline void exponential_moving_average(FloatT& accumulator, FloatT new_value, FloatT alpha)
{
    accumulator = (alpha * new_value) + (FloatT(1) - alpha) * accumulator;
}

/**
 * @brief Calculate a moving maximum that decays over time
 *
 * @note The value computed by this function will often be slightly lower than the actual hard maximum in the dataset
 *
 * @tparam FloatT
 * @param current_max A reference to the current maximum value, which will be updated
 * @param new_value The new value to consider
 * @param delta_time The time elapsed since the last update
 * @param half_life The half-life for the decay, which determines how quickly the maximum decreases over time
 * @param smoothing_factor Determines how quickly current_max approaches the hard maximum (0 = no smoothing, more
 * responsive, more jittery, 1 = max smoothing, less responsive)
 * @return requires
 */
template <typename FloatT = float>
    requires std::floating_point<FloatT>
void moving_maximum(FloatT& current_max, FloatT new_value, FloatT delta_time, FloatT half_life, FloatT smoothing_factor)
{
    // Decay the current maximum based on time elapsed since last update
    FloatT decay_rate = std::log(FloatT(2)) / half_life;
    current_max *= std::exp(-decay_rate * delta_time);
    // Calculate the potential new maximum
    FloatT potential_max = std::max(current_max, new_value);
    // Apply smoothing between the current max and the potential new max
    smoothing_factor = std::clamp(smoothing_factor, 0.f, 0.95f);
    current_max = current_max + (FloatT(1) - smoothing_factor) * (potential_max - current_max);
}

} // namespace math
} // namespace kb