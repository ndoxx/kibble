#include "math/numeric.h"

namespace kb
{
namespace math
{

std::pair<float, float> newton_raphson(std::function<float(float)> f_over_fprime, float xx, float epsilon,
                                       size_t max_iter)
{
    float hh = f_over_fprime(xx);
    size_t iter = 0;
    while (std::abs(hh) > epsilon && iter < max_iter)
    {
        hh = f_over_fprime(xx);
        xx -= hh;
        ++iter;
    }
    return {xx, hh};
}

float nr_initial_guess_iterative(std::function<float(float)> f, float start_x, float start_step, float alpha)
{
    // Dilate step each iteration, break when sign has changed
    float xx = start_x;
    float step = start_step;
    float yy = f(xx);
    float yy_prev = yy;
    while (yy * yy_prev > 0)
    {
        yy_prev = yy;
        xx += step;
        step *= alpha;
        yy = f(xx);
    }

    // Backtrack a bit
    return xx - 0.5f * step / alpha;
}

float integrate_simpson(std::function<float(float)> f, float lb, float ub, uint32_t subdivisions)
{
    // * Simpson's rule is more accurate if we subdivide the interval of integration
    float h = float(ub - lb) / float(subdivisions), // width of subdivisions
        sum_odd = 0.0f,                             // sum of odd subinterval contributions
        sum_even = 0.0f,                            // sum of even subinterval contributions
        y0 = f(lb),                                 // f value at lower bound
        yn = f(ub);                                 // f value at upper bound

    // loop to evaluate intermediary sums
    for (uint32_t ii = 1; ii < subdivisions; ++ii)
    {
        float yy = f(lb + float(ii) * h); // evaluate y_ii
        // sum of odd terms go into sum_odd and sum of even terms go into sum_even
        ((ii % 2) ? sum_odd : sum_even) += yy;
    }

    // h/3*[y0+yn+4*(y1+y3+y5+...+yn-1)+2*(y2+y4+y6+...+yn-2)]
    return h / 3.0f * (y0 + yn + 4.0f * sum_odd + 2.0f * sum_even);
}

} // namespace math
} // namespace kb