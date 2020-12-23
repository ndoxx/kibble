#pragma once
#include <cstdint>

namespace kb
{
namespace math
{

struct argb32_t
{
    static constexpr uint32_t k_amask = 0xFF000000;
    static constexpr uint32_t k_rmask = 0x00FF0000;
    static constexpr uint32_t k_gmask = 0x0000FF00;
    static constexpr uint32_t k_bmask = 0x000000FF;
    static constexpr uint32_t k_ashift = 24;
    static constexpr uint32_t k_rshift = 16;
    static constexpr uint32_t k_gshift = 8;
    static constexpr uint32_t k_bshift = 0;

    uint32_t value = 0xFFFFFFFF;

    constexpr uint32_t a() const { return (value & argb32_t::k_amask) >> argb32_t::k_ashift; }
    constexpr uint32_t r() const { return (value & argb32_t::k_rmask) >> argb32_t::k_rshift; }
    constexpr uint32_t g() const { return (value & argb32_t::k_gmask) >> argb32_t::k_gshift; }
    constexpr uint32_t b() const { return (value & argb32_t::k_bmask) >> argb32_t::k_bshift; }
    constexpr uint32_t operator[](int chan) { return (value & (uint32_t(0xFF) << (chan*8))) >> (chan*8); }
};

struct ColorRGBA;
struct ColorHSLA
{
    float h, s, l, a;

    constexpr ColorHSLA(float h_, float s_, float l_, float a_ = 1.f) : h(h_), s(s_), l(l_), a(a_) {}
    constexpr ColorHSLA() : ColorHSLA(0.f, 0.f, 0.f, 1.f) {}
    ColorHSLA(const ColorRGBA&);

    static ColorHSLA random_hue(float s = 1.f, float l = 0.5f, unsigned long long seed = 0);
};

struct ColorRGBA
{
    float r, g, b, a;

    constexpr ColorRGBA(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_) {}
    constexpr ColorRGBA() : ColorRGBA(0.f, 0.f, 0.f, 1.f) {}
    constexpr ColorRGBA(argb32_t color)
        : ColorRGBA(float(color.r()) / 255.f, float(color.g()) / 255.f, float(color.b()) / 255.f,
                    float(color.a()) / 255.f)
    {}
    ColorRGBA(const ColorHSLA&);

    template <typename Vec4T>
    constexpr ColorRGBA(const Vec4T& color): r(color[0]), g(color[1]), b(color[2]), a(color[3]) {}
};

struct ColorCIELab
{
    float L, a, b;

    constexpr ColorCIELab(float L_, float a_, float b_): L(L_), a(a_), b(b_) {}
    constexpr ColorCIELab() : ColorCIELab(0.f, 0.f, 0.f) {}
    ColorCIELab(const ColorRGBA& srgba);
    ColorCIELab(argb32_t color);
};

ColorRGBA to_RGBA(const ColorHSLA&);
ColorHSLA to_HSLA(const ColorRGBA&);
ColorCIELab to_CIELab(const ColorRGBA& srgba);

constexpr argb32_t pack_ARGB(uint8_t R, uint8_t G, uint8_t B, uint32_t A = 255)
{
    return {(uint32_t(A) << argb32_t::k_ashift) | (uint32_t(R) << argb32_t::k_rshift) |
            (uint32_t(G) << argb32_t::k_gshift) | (uint32_t(B) << argb32_t::k_bshift)};
}

constexpr argb32_t pack_ARGB(const ColorRGBA& rgba)
{
    return {uint32_t(255 * rgba.a) << argb32_t::k_ashift | uint32_t(255 * rgba.r) << argb32_t::k_rshift |
            uint32_t(255 * rgba.g) << argb32_t::k_gshift | uint32_t(255 * rgba.b) << argb32_t::k_bshift};
}

// * Color differences
// Fast perceptive difference, adapted from https://www.compuphase.com/cmetric.htm
float delta_E_cmetric(argb32_t C1, argb32_t C2);
// CIE delta E squared, 1976 formula (Lab space Euclidean distance)
float delta_E2_CIE76(ColorCIELab col1, ColorCIELab col2);
// CIE delta E squared, 1994 formula (L*C*h* distance that addresses perceptual non-uniformities)
float delta_E2_CIE94(ColorCIELab col1, ColorCIELab col2);

} // namespace math
} // namespace kb