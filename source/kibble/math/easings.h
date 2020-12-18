#pragma once
#include <cmath>

namespace kb
{
namespace easing
{

// * Utility
constexpr float flip(float t) { return 1.f - t; }
constexpr float mix(float a, float b, float t) { return (1.f - t) * a + t * b; }
constexpr float crossfade(float a, float b, float t) { return a + t * (b - a); }
constexpr float scale(float f, float t) { return t * f; }
constexpr float reverse_scale(float f, float t) { return (1.f - t) * f; }

// * Normalized easing functions ([0,1]->[0,1])
// Quadratic
constexpr float in_2(float t) { return t * t; }
constexpr float out_2(float t) { return flip(in_2(flip(t))); } // <=> return t*(2.f-t);
constexpr float inout_2(float t) { return crossfade(in_2(t), out_2(t), t); }

// Cubic
constexpr float in_3(float t) { return t * t * t; }
constexpr float out_3(float t) { return flip(in_3(flip(t))); } // <=> return (t-1.f)*(t-1.f)*(t-1.f)+1.f;
constexpr float inout_3(float t)
{
    return t < .5f ? 4.f * t * t * t : (t - 1.f) * (2.f * t - 2.f) * (2.f * t - 2.f) + 1.f;
}
// constexpr float inout_3(float t)   { return crossfade(in_3(t), out_3(t), t); }     // DNW

// Quartic
constexpr float in_4(float t) { return t * t * t * t; }
constexpr float out_4(float t) { return flip(in_4(flip(t))); } // <=> return 1.f-(t-1.f)*(t-1.f)*(t-1.f)*(t-1.f);
constexpr float inout_4(float t)
{
    return t < .5f ? 8.f * t * t * t * t : 1.f - 8.f * (t - 1.f) * (t - 1.f) * (t - 1.f) * (t - 1.f);
}
// constexpr float inout_4(float t) { return crossfade(in_4(t), out_4(t), t); } // DNW

// Quintic
constexpr float in_5(float t) { return t * t * t * t * t; }
constexpr float out_5(float t) { return flip(in_5(flip(t))); } // <=> return 1.f+(t-1.f)*(t-1.f)*(t-1.f)*(t-1.f)*(t-1.f);
constexpr float inout_5(float t)
{
    return t < .5f ? 16.f * t * t * t * t * t : 1.f + 16.f * (t - 1.f) * (t - 1.f) * (t - 1.f) * (t - 1.f) * (t - 1.f);
}
// constexpr float inout_5(float t) { return crossfade(in_5(t), out_5(t), t); } // DNW

// Bezier
constexpr float bezier_3(float b, float c, float t)
{
    float s = 1.f - t;
    float s2 = s * s;
    float t2 = t * t;
    float t3 = t2 * t;
    return (3.f * b * s2 * t) + (3.f * c * s * t2) + t3;
}

// Concave
constexpr float arch_2(float t) { return 4.f * scale(flip(t), t); }
constexpr float in_arch_3(float t) { return (27.f / 16.f) * scale(arch_2(t), t); }
constexpr float out_arch_3(float t) { return (27.f / 16.f) * reverse_scale(arch_2(t), t); }
constexpr float inout_arch4(float t) { return 4.f * reverse_scale(scale(arch_2(t), t), t); }
constexpr float bell_6(float t) { return 64.f * in_3(t) * flip(out_3(t)); }

// Bounce
constexpr float bounce_clamp_bottom(float t) { return std::abs(t); }
constexpr float bounce_clamp_top(float t) { return flip(bounce_clamp_bottom(flip(t))); }
constexpr float bounce_clamp_bottom_top(float t) { return bounce_clamp_top(bounce_clamp_bottom(t)); }

// just a test
constexpr float bounce_bezier_3(float t) { return bounce_clamp_top(bezier_3(4.f, -0.5f, t)); }

} // namespace easing
} // namespace kb