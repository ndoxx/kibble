#pragma once

#include <functional>
#include <utility>

namespace kb
{
namespace math
{

// Iterate Newton-Raphson algorithm untill error w.r.t root of function f less than epsilon or max iterations reached
// return root approximation and error
std::pair<float, float> newton_raphson(std::function<float(float)> f_over_fprime, float xx, float epsilon, size_t max_iter);
// Helper method to find the initial guess of Newton-Raphson
float nr_initial_guess_iterative(std::function<float(float)> f, float start_x, float start_step, float alpha = 2.f);

} // namespace math
} // namespace kb