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
    while(std::abs(hh) > epsilon && iter < max_iter)
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
    while(yy * yy_prev > 0)
    {
        yy_prev = yy;
        xx += step;
        step *= alpha;
        yy = f(xx);
    }
    return xx - 0.5f * step / alpha;
}

} // namespace math
} // namespace kb