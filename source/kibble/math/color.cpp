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

ColorCIELab::ColorCIELab(const ColorRGBA& rgba) : ColorCIELab(to_CIELab(rgba)) {}
ColorCIELab::ColorCIELab(argb32_t color) : ColorCIELab(to_CIELab(color)) {}

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

ColorCIELab to_CIELab(const ColorRGBA& srgba)
{
    // Gamma expand
    float lin_r = (srgba.r < 0.04045f) ? srgba.r / 12.92f : std::pow((srgba.r + 0.055f) / 1.055f, 2.4f);
    float lin_g = (srgba.g < 0.04045f) ? srgba.g / 12.92f : std::pow((srgba.g + 0.055f) / 1.055f, 2.4f);
    float lin_b = (srgba.b < 0.04045f) ? srgba.b / 12.92f : std::pow((srgba.b + 0.055f) / 1.055f, 2.4f);

    // Convert to XYZ
    float X = (0.41239080f * lin_r + 0.35758434f * lin_g + 0.18048079f * lin_b) / 0.95047f; // D65 illuminant
    float Y = (0.21263901f * lin_r + 0.71516868f * lin_g + 0.07219232f * lin_b) / 1.f;
    float Z = (0.01933082f * lin_r + 0.11919478f * lin_g + 0.95053215f * lin_b) / 1.08883f;

    // Convert to CIELab
    X = (X > 0.008856452f) ? std::pow(X, 1.f / 3.f) : 7.787037058f * X + 0.137931034f;
    Y = (Y > 0.008856452f) ? std::pow(Y, 1.f / 3.f) : 7.787037058f * Y + 0.137931034f;
    Z = (Z > 0.008856452f) ? std::pow(Z, 1.f / 3.f) : 7.787037058f * Z + 0.137931034f;

    return ColorCIELab(116.f * Y - 16.f, 500.f * (X - Y), 200.f * (Y - Z));
}

float delta_E_cmetric(argb32_t C1, argb32_t C2)
{
    int rmean = (int(C1.r()) + int(C2.r())) / 2;
    int r = int(C1.r()) - int(C2.r());
    int g = int(C1.g()) - int(C2.g());
    int b = int(C1.b()) - int(C2.b());
    return float(std::sqrt((((512 + rmean) * r * r) >> 8) + 4 * g * g + (((767 - rmean) * b * b) >> 8)));
}

// Sources:
// https://en.wikipedia.org/wiki/Color_difference
// http://www.brucelindbloom.com/index.html?Equations.html
// http://www.easyrgb.com/en/math.php
float delta_E2_CIE76(ColorCIELab col1, ColorCIELab col2)
{
    return (col2.L - col1.L) * (col2.L - col1.L) + (col2.a - col1.a) * (col2.a - col1.a) +
           (col2.b - col1.b) * (col2.b - col1.b);
}

float delta_E2_CIE94(ColorCIELab col1, ColorCIELab col2)
{
    constexpr float k_L = 1.f;
    constexpr float S_L = 1.f;
    constexpr float K_1 = 0.045f;
    constexpr float K_2 = 0.015f;

    float C1 = std::sqrt(col1.a * col1.a + col1.b * col1.b);
    float C2 = std::sqrt(col2.a * col2.a + col2.b * col2.b);
    float S_C = 1.f + K_1 * C1;
    float S_H = 1.f + K_2 * C1;
    float da = (col1.a - col2.a);
    float db = (col1.b - col2.b);
    float dL = (col1.L - col2.L);
    float dC = (C1 - C2);
    float dH = std::sqrt(da * da + db * db - dC * dC);

    dL /= (k_L * S_L);
    dC /= S_C;
    dH /= S_H;

    return dL * dL + dC * dC + dH * dH;
}

} // namespace math
} // namespace kb