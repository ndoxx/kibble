#include "math/catenary.h"
#include "math/numeric.h"

namespace kb
{
namespace math
{

constexpr float k_min_slack = 0.01f;

inline float coth(float x) { return std::cosh(x) / std::sinh(x); }
inline float argsinh(float x) { return std::log(x + std::sqrt(1.f + x * x)); }

Catenary::Catenary(float x1, float y1, float x2, float y2, float s, float max_error)
{
    // * Preparatory steps
    // The equations are solved for the specific case y1<y2 and x1<x2
    // If this is not the case, we may need to swap terms and reflect the catenary curve
    if(y2 < y1)
    {
        std::swap(y1, y2);
        reflect_ = true;
    }
    if(x2 < x1)
    {
        std::swap(x1, x2);
        reflect_ = !reflect_;
    }

    // Auxillary
    float v = y2 - y1;
    float h = x2 - x1;
    m_ = (x2 + x1);

    // Check that specified length is larger than distance between anchor points
    // if not, set length to distance plus some slack
    float dist2 = v * v + h * h;
    if(s * s < dist2 + k_min_slack)
        s = std::sqrt(dist2) + k_min_slack;

    // * Solve for parameter a_, this equates to solving a transcendental equation for a unique root
    // Run along the function graph till the sign changes, this provides a good initial guess for the
    // Newton-Raphson step
    float k = std::sqrt(s * s - v * v);
    auto f = [h, k](float x) { return 2.f * x * std::sinh(0.5f * h / x) - k; };
    float x0 = math::nr_initial_guess_iterative(f, 0.1f, 0.01f, 1.8f);
    // Iterate Newton-Raphson algorithm
    auto f_over_fprime = [h, k](float x) {
        return (x * (2.f * x * std::sinh(0.5f * h / x) - k)) /
               (2.f * x * std::sinh(0.5f * h / x) - h * std::cosh(0.5f * h / x));
    };
    auto&& [aa, _] = math::newton_raphson(f_over_fprime, x0, max_error, 20);
    a_ = aa;

    // * Solve for p and q, there is a closed form
    p_ = 0.5f * (x1 + x2 - a_ * std::log((s + v) / (s - v)));
    q_ = 0.5f * (y1 + y2 - s * coth(0.5f * h / a_));
    C_ = a_ * std::sinh((x1 - p_) / a_);
    s_ = s;
}

float Catenary::arclen_remap(float ss) const
{
    // Input parameter between 0 and 1
    // Target length between 0 and s_
    ss = s_ * std::clamp(ss, 0.f, 1.f);
    // Inverse arc-length function (calculation in my notes)
    return a_ * argsinh((ss + C_) / a_) + p_;
}

} // namespace math
} // namespace kb