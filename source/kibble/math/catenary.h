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
    inline float value_arclen(float ss) const { return value(arclen_remap(ss)); }
    // Return arc-length parameterized first derivative
    inline float prime_arclen(float ss) const { return prime(arclen_remap(ss)); }
    // Return the value of the catenary argument for specified length fraction ss in [0,1]
    float arclen_remap(float ss) const;

private:
    float a_; // Scale parameter
    float p_; // x-offset
    float q_; // y-offset
    float m_; // Anchors midpoint
    float C_; // Integration constant for arc-length parameterization
    float s_; // Full length between anchor points
    bool reflect_ = false; // Reflect curve w.r.t midpoint?
};

} // namespace math
} // namespace kb