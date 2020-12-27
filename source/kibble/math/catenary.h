#pragma once
#include <cmath>

namespace kb
{
namespace math
{

class Catenary
{
public:
    Catenary(float x1, float y1, float x2, float y2, float s, float max_error = 0.001f);

    inline float value(float xx) const { return a_ * std::cosh(((reflect_ ? -xx + m_ : xx) - p_) / a_) + q_; }
    inline float prime(float xx) const { return std::sinh(((reflect_ ? -xx + m_ : xx) - p_) / a_); }

private:
    float a_;
    float p_;
    float q_;
    float m_;
    bool reflect_ = false;
};

} // namespace math
} // namespace kb