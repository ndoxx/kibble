#pragma once
#include <cmath>
#include <numbers>
#include <type_traits>

namespace kb
{
namespace ease
{

namespace traits
{
template <typename FloatT>
using enable_if_float = std::enable_if_t<std::is_floating_point_v<FloatT>>;
}

// * Utility
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT flip(FloatT t)
{
    return FloatT(1) - t;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT mix(FloatT a, FloatT b, FloatT t)
{
    return (FloatT(1) - t) * a + t * b;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT crossfade(FloatT a, FloatT b, FloatT t)
{
    return a + t * (b - a);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT scale(FloatT f, FloatT t)
{
    return t * f;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT reverse_scale(FloatT f, FloatT t)
{
    return (FloatT(1) - t) * f;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT bounce_clamp_bottom(FloatT t)
{
    return std::abs(t);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT bounce_clamp_top(FloatT t)
{
    return flip(bounce_clamp_bottom(flip(t)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT bounce_clamp_bottom_top(FloatT t)
{
    return bounce_clamp_top(bounce_clamp_bottom(t));
}

// * Normalized easing functions ([0,1]->[0,1])
// Trigonometric
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT in_sine(FloatT t)
{
    return FloatT(1) - std::cos(FloatT(0.5) * (t * std::numbers::pi_v<FloatT>));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT out_sine(FloatT t)
{
    return std::sin(FloatT(0.5) * (t * std::numbers::pi_v<FloatT>));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT inout_sine(FloatT t)
{
    return -FloatT(0.5) * (std::cos(std::numbers::pi_v<FloatT> * t) - FloatT(1));
}

// Exponential
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT in_exp(FloatT t)
{
    return t == FloatT(0) ? FloatT(0) : std::pow(FloatT(2), FloatT(10) * t - FloatT(10));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT out_exp(FloatT t)
{
    return t == FloatT(1) ? FloatT(1) : FloatT(1) - std::pow(FloatT(2), -FloatT(10) * t);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT inout_exp(FloatT t)
{
    return t == FloatT(0)    ? FloatT(0)
           : t == FloatT(1)  ? FloatT(1)
           : t < FloatT(0.5) ? FloatT(0.5) * std::pow(FloatT(2), FloatT(20) * t - FloatT(10))
                             : FloatT(0.5) * (FloatT(2) - std::pow(FloatT(2), -FloatT(20) * t + FloatT(10)));
}

// Radical
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT in_circ(FloatT t)
{
    return FloatT(1) - std::sqrt(FloatT(1) - (t * t));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT out_circ(FloatT t)
{
    return std::sqrt(FloatT(1) - (t - FloatT(1)) * (t - FloatT(1)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
inline FloatT inout_circ(FloatT t)
{
    return t < FloatT(0.5)
               ? FloatT(0.5) * (FloatT(1) - std::sqrt(FloatT(1) - FloatT(4) * t * t))
               : FloatT(0.5) *
                     (std::sqrt(FloatT(1) - (-FloatT(2) * t + FloatT(2)) * (-FloatT(2) * t + FloatT(2))) + FloatT(1));
}

// Quadratic
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT in_2(FloatT t)
{
    return t * t;
}
// == t*(FloatT(2)-t);
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT out_2(FloatT t)
{
    return flip(in_2(flip(t)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT inout_2(FloatT t)
{
    return crossfade(in_2(t), out_2(t), t);
}

// Cubic
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT in_3(FloatT t)
{
    return t * t * t;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT out_3(FloatT t)
{
    return flip(in_3(flip(t)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT inout_3(FloatT t)
{
    return t < FloatT(0.5) ? FloatT(4) * t * t * t
                           : (t - FloatT(1)) * (FloatT(2) * t - FloatT(2)) * (FloatT(2) * t - FloatT(2)) + FloatT(1);
}

// Quartic
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT in_4(FloatT t)
{
    return t * t * t * t;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT out_4(FloatT t)
{
    return flip(in_4(flip(t)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT inout_4(FloatT t)
{
    return t < FloatT(0.5)
               ? FloatT(8) * t * t * t * t
               : FloatT(1) - FloatT(8) * (t - FloatT(1)) * (t - FloatT(1)) * (t - FloatT(1)) * (t - FloatT(1));
}

// Quintic
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT in_5(FloatT t)
{
    return t * t * t * t * t;
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT out_5(FloatT t)
{
    return flip(in_5(flip(t)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT inout_5(FloatT t)
{
    return t < FloatT(0.5) ? FloatT(16) * t * t * t * t * t
                           : FloatT(1) + FloatT(16) * (t - FloatT(1)) * (t - FloatT(1)) * (t - FloatT(1)) *
                                             (t - FloatT(1)) * (t - FloatT(1));
}

// Bezier
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT bezier_3(FloatT b, FloatT c, FloatT t)
{
    FloatT s = FloatT(1) - t;
    FloatT s2 = s * s;
    FloatT t2 = t * t;
    FloatT t3 = t2 * t;
    return (FloatT(3) * b * s2 * t) + (FloatT(3) * c * s * t2) + t3;
}

// Concave
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT arch_2(FloatT t)
{
    return FloatT(4) * scale(flip(t), t);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT in_arch_3(FloatT t)
{
    return (FloatT(27) / FloatT(16)) * scale(arch_2(t), t);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT out_arch_3(FloatT t)
{
    return (FloatT(27) / FloatT(16)) * reverse_scale(arch_2(t), t);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT inout_arch4(FloatT t)
{
    return FloatT(4) * reverse_scale(scale(arch_2(t), t), t);
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT bell_6(FloatT t)
{
    return FloatT(64) * in_3(t) * flip(out_3(t));
}

// Bounce
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT out_bounce_bezier_3(FloatT t)
{
    return bounce_clamp_top(bezier_3(FloatT(4), -FloatT(0.5), t));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT in_bounce_bezier_3(FloatT t)
{
    return flip(out_bounce_bezier_3(flip(t)));
}
template <typename FloatT, typename = traits::enable_if_float<FloatT>>
constexpr FloatT inout_bounce_bezier_3(FloatT t)
{
    return t < FloatT(0.5) ? FloatT(0.5) * (FloatT(1) - out_bounce_bezier_3(FloatT(1) - FloatT(2) * t))
                           : FloatT(0.5) * (FloatT(1) + out_bounce_bezier_3(FloatT(2) * t - FloatT(1)));
}

} // namespace ease
} // namespace kb