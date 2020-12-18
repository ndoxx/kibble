#include "math/color.h"

#include <array>
#include <cmath>
#include <random>

namespace kb
{
namespace math
{

ColorRGBA::ColorRGBA(const ColorHSLA& hsla) : ColorRGBA(to_RGBA(hsla)) {}

ColorHSLA::ColorHSLA(const ColorRGBA& rgba) : ColorHSLA(to_HSLA(rgba)) {}

ColorHSLA ColorHSLA::random_hue(float s, float l, unsigned long long seed)
{
    static std::random_device r;
    static std::default_random_engine generator(seed ? seed : r());
    std::uniform_real_distribution<float> distribution(0.f, 1.f);

    return ColorHSLA(distribution(generator), s, l);
}

constexpr float SQ3_2 = 0.866025404f;
constexpr std::array<float, 3> W = {0.2126f, 0.7152f, 0.0722f};

ColorRGBA to_RGBA(const ColorHSLA& hsla)
{
    // Chroma
    float c = (1.0f - std::abs(2.0f * hsla.l - 1.0f)) * hsla.s;
    // Intermediate
    float Hp = hsla.h * 6.0f; // Map to degrees -> *360°, divide by 60° -> *6
    float x = c * (1.0f - std::abs(fmodf(Hp, 2) - 1.0f));
    uint8_t hn = uint8_t(std::floor(Hp));

    // Lightness offset
    float m = hsla.l - 0.5f * c;

    // Project to RGB cube
    switch(hn)
    {
    case 0:
        return ColorRGBA(c + m, x + m, m, hsla.a);
    case 1:
        return ColorRGBA(x + m, c + m, m, hsla.a);
    case 2:
        return ColorRGBA(m, c + m, x + m, hsla.a);
    case 3:
        return ColorRGBA(m, x + m, c + m, hsla.a);
    case 4:
        return ColorRGBA(x + m, m, c + m, hsla.a);
    case 5:
        return ColorRGBA(c + m, m, x + m, hsla.a);
    default:
        return ColorRGBA(m, m, m, hsla.a);
    }
}

ColorHSLA to_HSLA(const ColorRGBA& rgba)
{
    // Auxillary Cartesian chromaticity coordinates
    float a = 0.5f * (2.0f * rgba.r - rgba.g - rgba.b);
    float b = SQ3_2 * (rgba.g - rgba.b);
    // Hue
    float h = std::atan2(b, a);
    // Chroma
    float c = std::sqrt(a * a + b * b);
    // We choose Luma Y_709 (for sRGB primaries) as a definition for Lightness
    float l = rgba.r * W[0] + rgba.g * W[1] + rgba.b * W[1];
    // Saturation
    float s = (l == 1.0f) ? 0.0f : c / (1.0f - std::abs(2 * l - 1));
    // Just convert hue from radians in [-pi/2,pi/2] to [0,1]
    return ColorHSLA(h / float(M_PI) + 0.5f, s, l, rgba.a);
}

} // namespace math
} // namespace kb