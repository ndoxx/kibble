#pragma once
#include <cmath>

namespace kb
{
namespace math
{

class Catenary
{
public:
    // Construct a catenary curve between anchor points (x1,y1) and (x2,y2), of length s
    // max_error is the maximal error for parameter estimation
    Catenary(float x1, float y1, float x2, float y2, float s, float max_error = 0.001f);

    // Return the value along the curve at specified parameter value
    inline float value(float xx) const { return a_ * std::cosh(((reflect_ ? -xx + m_ : xx) - p_) / a_) + q_; }
    // Return the first derivative at specified parameter value
    inline float prime(float xx) const { return std::sinh(((reflect_ ? -xx + m_ : xx) - p_) / a_); }
    // Return arc-length parameterized value
    inline float value_alp(float ss) const { return value(arclen_remap(ss)); }
    // Return arc-length parameterized first derivative
    inline float prime_alp(float ss) const { return prime(arclen_remap(ss)); }
    // Return the value of the catenary argument for specified length fraction ss in [0,1]
    float arclen_remap(float ss) const;

private:
    float a_;
    float p_;
    float q_;
    float m_;
    float C_;
    float s_;
    bool reflect_ = false;
};

} // namespace math
} // namespace kb