#include "math/color.h"

#include <array>
#include <cmath>
#include <random>

namespace kb
{
namespace math
{

argb32_t lighten(argb32_t color, float factor)
{
    factor = std::clamp(factor, 0.f, 1.f);

    uint8_t R = uint8_t(std::roundf(factor * float(color.r())));
    uint8_t G = uint8_t(std::roundf(factor * float(color.g())));
    uint8_t B = uint8_t(std::roundf(factor * float(color.b())));

    return pack_ARGB(R, G, B);
}

ColorRGBA::ColorRGBA(const ColorHSLA& hsla) : ColorRGBA(to_RGBA(hsla))
{
}

ColorHSLA::ColorHSLA(const ColorRGBA& rgba) : ColorHSLA(to_HSLA(rgba))
{
}

ColorCIELab::ColorCIELab(const ColorRGBA& rgba) : ColorCIELab(to_CIELab(rgba))
{
}
ColorCIELab::ColorCIELab(argb32_t color) : ColorCIELab(to_CIELab(color))
{
}

ColorHSLA ColorHSLA::random_hue(float s, float l, unsigned long long seed)
{
    static std::random_device r;
    static std::default_random_engine generator(seed ? seed : r());
    std::uniform_real_distribution<float> distribution(0.f, 1.f);

    return ColorHSLA(distribution(generator), s, l);
}

float hue_to_rgb(float v1, float v2, float vH)
{
    if (vH < 0)
        vH += 1;
    if (vH > 1)
        vH -= 1;
    if ((6 * vH) < 1)
        return (v1 + (v2 - v1) * 6.f * vH);
    if ((2 * vH) < 1)
        return (v2);
    if ((3 * vH) < 2)
        return (v1 + (v2 - v1) * ((2.f / 3.f) - vH) * 6);
    return (v1);
}

ColorRGBA to_RGBA(const ColorHSLA& hsla)
{
    if (hsla.s == 0)
        return math::ColorRGBA(hsla.l, hsla.l, hsla.l);
    else
    {
        float v1 = 0.f;
        float v2 = 0.f;
        if (hsla.l < 0.5f)
            v2 = hsla.l * (1 + hsla.s);
        else
            v2 = (hsla.l + hsla.s) - (hsla.s * hsla.l);

        v1 = 2 * hsla.l - v2;

        float R = hue_to_rgb(v1, v2, hsla.h + (1.f / 3.f));
        float G = hue_to_rgb(v1, v2, hsla.h);
        float B = hue_to_rgb(v1, v2, hsla.h - (1.f / 3.f));
        return math::ColorRGBA(R, G, B, hsla.a);
    }
}

ColorHSLA to_HSLA(const ColorRGBA& rgba)
{
    float cmin = std::min(rgba.r, std::min(rgba.g, rgba.b));
    float cmax = std::max(rgba.r, std::max(rgba.g, rgba.b));
    float delta = cmax - cmin;
    float H = 0.f;
    float S = 0.f;
    float L = 0.5f * (cmax + cmin);

    if (delta > 0)
    {
        S = (L < 0.5f) ? delta / (cmax + cmin) : delta / (2.f - cmax - cmin);
        float del_R = (((cmax - rgba.r) / 6.f) + (delta * 0.5f)) / delta;
        float del_G = (((cmax - rgba.g) / 6.f) + (delta * 0.5f)) / delta;
        float del_B = (((cmax - rgba.b) / 6.f) + (delta * 0.5f)) / delta;
        if (rgba.r == cmax)
            H = del_B - del_G;
        else if (rgba.g == cmax)
            H = (1.f / 3.f) + del_R - del_B;
        else if (rgba.b == cmax)
            H = (2.f / 3.f) + del_G - del_R;
        if (H < 0)
            H += 1;
        if (H > 1)
            H -= 1;
    }
    return math::ColorHSLA(H, S, L, rgba.a);
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