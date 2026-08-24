#include "kibble/math/color.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace kb::math
{

/*
    Resources:
    https://en.wikipedia.org/wiki/Color_difference
    http://www.brucelindbloom.com/index.html?Equations.html
    http://www.easyrgb.com/en/math.php
*/

// NOTE: these are the CIE 1931 D65 illuminants
constexpr float Xr_D65 = 0.95047f;
constexpr float Yr_D65 = 1.f;
constexpr float Zr_D65 = 1.08883f;

float hue_to_rgb(float v1, float v2, float vH)
{
    if (vH < 0)
    {
        vH += 1;
    }
    if (vH > 1)
    {
        vH -= 1;
    }
    if ((6 * vH) < 1)
    {
        return (v1 + (v2 - v1) * 6.f * vH);
    }
    if ((2 * vH) < 1)
    {
        return (v2);
    }
    if ((3 * vH) < 2)
    {
        return (v1 + (v2 - v1) * ((2.f / 3.f) - vH) * 6);
    }
    return (v1);
}

ColorRGBA argb32_t::to_rgba() const
{
    return ColorRGBA{float(r()) / 255.f, float(g()) / 255.f, float(b()) / 255.f, float(a()) / 255.f};
}

ColorHSLA ColorRGBA::to_hsla() const
{
    float cmin = std::min(r, std::min(g, b));
    float cmax = std::max(r, std::max(g, b));
    float delta = cmax - cmin;
    float H = 0.f;
    float S = 0.f;
    float L = 0.5f * (cmax + cmin);

    if (delta > 0)
    {
        S = (L < 0.5f) ? delta / (cmax + cmin) : delta / (2.f - cmax - cmin);
        float del_R = (((cmax - r) / 6.f) + (delta * 0.5f)) / delta;
        float del_G = (((cmax - g) / 6.f) + (delta * 0.5f)) / delta;
        float del_B = (((cmax - b) / 6.f) + (delta * 0.5f)) / delta;
        if (r == cmax)
        {
            H = del_B - del_G;
        }
        else if (g == cmax)
        {
            H = (1.f / 3.f) + del_R - del_B;
        }
        else if (b == cmax)
        {
            H = (2.f / 3.f) + del_G - del_R;
        }
        if (H < 0)
        {
            H += 1;
        }
        if (H > 1)
        {
            H -= 1;
        }
    }
    return math::ColorHSLA{H, S, L, a};
}

ColorCIELab ColorRGBA::to_CIELab() const
{
    // Gamma expand
    float lin_r = (r < 0.04045f) ? r / 12.92f : std::pow((r + 0.055f) / 1.055f, 2.4f);
    float lin_g = (g < 0.04045f) ? g / 12.92f : std::pow((g + 0.055f) / 1.055f, 2.4f);
    float lin_b = (b < 0.04045f) ? b / 12.92f : std::pow((b + 0.055f) / 1.055f, 2.4f);

    // Convert to XYZ
    float X = (0.41239080f * lin_r + 0.35758434f * lin_g + 0.18048079f * lin_b) / Xr_D65;
    float Y = (0.21263901f * lin_r + 0.71516868f * lin_g + 0.07219232f * lin_b) / Yr_D65;
    float Z = (0.01933082f * lin_r + 0.11919478f * lin_g + 0.95053215f * lin_b) / Zr_D65;

    // Convert to CIELab
    X = (X > 0.008856452f) ? std::pow(X, 1.f / 3.f) : 7.787037058f * X + 0.137931034f;
    Y = (Y > 0.008856452f) ? std::pow(Y, 1.f / 3.f) : 7.787037058f * Y + 0.137931034f;
    Z = (Z > 0.008856452f) ? std::pow(Z, 1.f / 3.f) : 7.787037058f * Z + 0.137931034f;

    return ColorCIELab{116.f * Y - 16.f, 500.f * (X - Y), 200.f * (Y - Z)};
}

argb32_t ColorRGBA::to_argb32() const
{
    return {(uint32_t(std::roundf(std::clamp(r, 0.f, 1.f) * 255.f)) << argb32_t::k_rshift) |
            (uint32_t(std::roundf(std::clamp(g, 0.f, 1.f) * 255.f)) << argb32_t::k_gshift) |
            (uint32_t(std::roundf(std::clamp(b, 0.f, 1.f) * 255.f)) << argb32_t::k_bshift) |
            (uint32_t(std::roundf(std::clamp(a, 0.f, 1.f) * 255.f)) << argb32_t::k_ashift)};
}

ColorRGBA ColorHSLA::to_rgba() const
{
    if (s == 0)
    {
        return ColorRGBA{l, l, l, 1.f};
    }
    else
    {
        float v1 = 0.f;
        float v2 = 0.f;
        if (l < 0.5f)
        {
            v2 = l * (1 + s);
        }
        else
        {
            v2 = (l + s) - (s * l);
        }

        v1 = 2 * l - v2;

        float R = hue_to_rgb(v1, v2, h + (1.f / 3.f));
        float G = hue_to_rgb(v1, v2, h);
        float B = hue_to_rgb(v1, v2, h - (1.f / 3.f));
        return ColorRGBA{R, G, B, a};
    }
}

ColorRGBA ColorCIELab::to_rgba() const
{
    // CIELab to XYZ
    float Y = (L + 16.f) / 116.f;
    float X = Y + a / 500.f;
    float Z = Y - b / 200.f;

    float X3 = X * X * X;
    float Y3 = Y * Y * Y;
    float Z3 = Z * Z * Z;
    X = X3 > 0.008856f ? X3 : (X - 16.f / 116.f) / 7.787f;
    Y = Y3 > 0.008856f ? Y3 : (Y - 16.f / 116.f) / 7.787f;
    Z = Z3 > 0.008856f ? Z3 : (Z - 16.f / 116.f) / 7.787f;

    X *= Xr_D65;
    Y *= Yr_D65;
    Z *= Zr_D65;

    // XYZ to RGB
    float R = 3.2406f * X - 1.5372f * Y - 0.4986f * Z;
    float G = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
    float B = 0.0557f * X - 0.2040f * Y + 1.0570f * Z;

    R = R > 0.0031308f ? 1.055f * std::pow(R, 1.f / 2.4f) - 0.055f : 12.92f * R;
    G = G > 0.0031308f ? 1.055f * std::pow(G, 1.f / 2.4f) - 0.055f : 12.92f * G;
    B = B > 0.0031308f ? 1.055f * std::pow(B, 1.f / 2.4f) - 0.055f : 12.92f * B;

    return ColorRGBA{R, G, B, 1.f};
}

argb32_t lerp(argb32_t col1, argb32_t col2, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    auto lerp_chan = [t](uint32_t a, uint32_t b) -> uint32_t {
        return uint32_t(std::roundf(float(a) + t * float(int32_t(b) - int32_t(a))));
    };
    return argb32_t::pack(lerp_chan(col1.r(), col2.r()), lerp_chan(col1.g(), col2.g()), lerp_chan(col1.b(), col2.b()),
                          lerp_chan(col1.a(), col2.a()));
}

argb32_t modulate_alpha(argb32_t color, float alpha)
{
    alpha = std::clamp(alpha, 0.f, 1.f);
    uint32_t alpha_chan = std::clamp(uint32_t(alpha * float(color.a())), 0u, 255u);
    color.value = (color.value & ~argb32_t::k_amask) | (alpha_chan << argb32_t::k_ashift);
    return color;
}

argb32_t lighten_darken_rgb_fast(argb32_t color, float factor)
{
    factor = std::clamp(factor, 0.f, 1.f);

    uint8_t R = uint8_t(std::roundf(factor * float(color.r())));
    uint8_t G = uint8_t(std::roundf(factor * float(color.g())));
    uint8_t B = uint8_t(std::roundf(factor * float(color.b())));

    return argb32_t::pack(R, G, B);
}

argb32_t lighten_darken_rgb(argb32_t color, float factor)
{
    constexpr float k_threshold = 255.999f;
    factor = std::abs(factor);

    // Extract RGB components
    float r = float(color.r()) * factor;
    float g = float(color.g()) * factor;
    float b = float(color.b()) * factor;
    uint32_t a = (color.value & argb32_t::k_amask) >> argb32_t::k_ashift;

    // Redistribute RGB values
    float m = std::fmax(r, std::fmax(g, b));

    if (m <= k_threshold)
    {
        // No redistribution needed
        return argb32_t::pack(static_cast<uint8_t>(std::fmin(r, 255.0f)), static_cast<uint8_t>(std::fmin(g, 255.0f)),
                              static_cast<uint8_t>(std::fmin(b, 255.0f)), a);
    }

    float total = r + g + b;
    if (total >= 3 * k_threshold)
    {
        // All channels at maximum
        return argb32_t::pack(255, 255, 255, a);
    }

    float x = (3 * k_threshold - total) / (3 * m - total);
    float gray = k_threshold - x * m;

    // Create and return new color
    return argb32_t::pack(static_cast<uint8_t>(gray + x * r), static_cast<uint8_t>(gray + x * g),
                          static_cast<uint8_t>(gray + x * b), a);
}

argb32_t lighten_darken(argb32_t color, float factor)
{
    ColorHSLA color_hsl = color.to_rgba().to_hsla();
    color_hsl.l = std::clamp(color_hsl.l * factor, 0.f, 1.f);
    return color_hsl.to_rgba().to_argb32();
}

ColorHSLA random_hue(float s, float l, unsigned long long seed)
{
    static std::random_device r;
    static std::default_random_engine generator(seed ? seed : r());
    std::uniform_real_distribution<float> distribution(0.f, 1.f);

    return ColorHSLA{distribution(generator), s, l, 1.f};
}

float delta_E_cmetric(argb32_t C1, argb32_t C2)
{
    int rmean = (int(C1.r()) + int(C2.r())) / 2;
    int r = int(C1.r()) - int(C2.r());
    int g = int(C1.g()) - int(C2.g());
    int b = int(C1.b()) - int(C2.b());
    return float(std::sqrt((((512 + rmean) * r * r) >> 8) + 4 * g * g + (((767 - rmean) * b * b) >> 8)));
}

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

} // namespace kb::math