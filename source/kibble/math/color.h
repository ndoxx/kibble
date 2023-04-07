#pragma once
#include <cstdint>

namespace kb
{
namespace math
{

/**
 * @brief Holds an ARGB color packed inside a 32b value
 *
 */
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

    /**
     * @brief Return the value of the alpha channel
     *
     * @return constexpr uint32_t
     */
    constexpr uint32_t a() const
    {
        return (value & argb32_t::k_amask) >> argb32_t::k_ashift;
    }

    /**
     * @brief Return the value of the red channel
     *
     * @return constexpr uint32_t
     */
    constexpr uint32_t r() const
    {
        return (value & argb32_t::k_rmask) >> argb32_t::k_rshift;
    }

    /**
     * @brief Return the value of the green channel
     *
     * @return constexpr uint32_t
     */
    constexpr uint32_t g() const
    {
        return (value & argb32_t::k_gmask) >> argb32_t::k_gshift;
    }

    /**
     * @brief Return the value of the blue channel
     *
     * @return constexpr uint32_t
     */
    constexpr uint32_t b() const
    {
        return (value & argb32_t::k_bmask) >> argb32_t::k_bshift;
    }

    /**
     * @brief Access a color channel by index
     *
     * @param chan
     * @return constexpr uint32_t
     */
    constexpr uint32_t operator[](int chan)
    {
        return (value & (uint32_t(0xFF) << (chan * 8))) >> (chan * 8);
    }
};

/**
 * @brief Transform a color by multiplication of each color channel by a given factor.
 * Allows to lighten or darken colors
 *
 * @param color Input color
 * @param factor Multiplicative factor, clamped between 0 and 1
 * @return argb32_t
 */
argb32_t lighten(argb32_t color, float factor);

struct ColorRGBA;

/**
 * @brief Represents a color in the HSLA color space.
 * Each channel is floating point.
 *
 */
struct ColorHSLA
{
    float h, s, l, a;

    /**
     * @brief Construct a color from its channels.
     *
     */
    constexpr ColorHSLA(float h_, float s_, float l_, float a_ = 1.f) : h(h_), s(s_), l(l_), a(a_)
    {
    }

    /**
     * @brief Default to black.
     *
     */
    constexpr ColorHSLA() : ColorHSLA(0.f, 0.f, 0.f, 1.f)
    {
    }

    /**
     * @brief Convert an RGBA color in place and create an HSLA color.
     *
     */
    ColorHSLA(const ColorRGBA&);

    /**
     * @brief Create an HSLA color of random hue.
     *
     * @param s Saturation
     * @param l Lightness
     * @param seed Seed for the RNG
     * @return ColorHSLA
     */
    static ColorHSLA random_hue(float s = 1.f, float l = 0.5f, unsigned long long seed = 0);
};

/**
 * @brief Represents an RGBA color.
 * Each channel is floating point.
 *
 */
struct ColorRGBA
{
    float r, g, b, a;

    /**
     * @brief Construct a color from its channels.
     *
     */
    constexpr ColorRGBA(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_)
    {
    }

    /**
     * @brief Default to black.
     *
     */
    constexpr ColorRGBA() : ColorRGBA(0.f, 0.f, 0.f, 1.f)
    {
    }

    /**
     * @brief Construct a color from an argb32_t packed color.
     *
     */
    constexpr ColorRGBA(argb32_t color)
        : ColorRGBA(float(color.r()) / 255.f, float(color.g()) / 255.f, float(color.b()) / 255.f,
                    float(color.a()) / 255.f)
    {
    }

    /**
     * @brief Convert an HSLA color in place and create an RGBA color.
     *
     */
    ColorRGBA(const ColorHSLA&);

    /**
     * @brief Construct an RGBA color from a generic vector type.
     *
     * @tparam Vec4T The vector type. Must define a bracket operator.
     */
    template <typename Vec4T>
    constexpr ColorRGBA(const Vec4T& color) : r(color[0]), g(color[1]), b(color[2]), a(color[3])
    {
    }
};

/**
 * @brief Represents a color in the CIELab color space.
 * Each channel is floating point.
 *
 */
struct ColorCIELab
{
    float L, a, b;

    /**
     * @brief Construct a color from its channels.
     *
     */
    constexpr ColorCIELab(float L_, float a_, float b_) : L(L_), a(a_), b(b_)
    {
    }

    /**
     * @brief Default to black.
     *
     */
    constexpr ColorCIELab() : ColorCIELab(0.f, 0.f, 0.f)
    {
    }

    /**
     * @brief Convert an RGBA color in place and construct a CIELab color.
     * The argument must be in the standard RGBA color space (sRGBA).
     *
     * @param srgba
     */
    ColorCIELab(const ColorRGBA& srgba);

    /**
     * @brief Convert an argb32_t color in place and construct a CIELab color.
     * The argument must be in the standard RGBA color space (sRGBA).
     *
     * @param color
     */
    ColorCIELab(argb32_t color);
};

/**
 * @brief Convert a color from HSLA space to RGBA.
 *
 * @return ColorRGBA
 */
ColorRGBA to_RGBA(const ColorHSLA&);

/**
 * @brief Convert a color from RGBA space to HSLA.
 *
 * @return ColorHSLA
 */
ColorHSLA to_HSLA(const ColorRGBA&);

/**
 * @brief Convert a color from sRGBA space to CIELab.
 *
 * @param srgba
 * @return ColorCIELab
 */
ColorCIELab to_CIELab(const ColorRGBA& srgba);

/**
 * @brief Build an argb32_t from RGBA channel values.
 *
 * @param R Red channel
 * @param G Green channel
 * @param B Blue channel
 * @param A Alpha channel, default to opaque
 * @return constexpr argb32_t
 */
constexpr argb32_t pack_ARGB(uint8_t R, uint8_t G, uint8_t B, uint32_t A = 255)
{
    return {(uint32_t(A) << argb32_t::k_ashift) | (uint32_t(R) << argb32_t::k_rshift) |
            (uint32_t(G) << argb32_t::k_gshift) | (uint32_t(B) << argb32_t::k_bshift)};
}

/**
 * @brief Build an argb32_t from a ColorRGBA
 *
 * @param rgba
 * @return constexpr argb32_t
 */
constexpr argb32_t pack_ARGB(const ColorRGBA& rgba)
{
    return {uint32_t(255 * rgba.a) << argb32_t::k_ashift | uint32_t(255 * rgba.r) << argb32_t::k_rshift |
            uint32_t(255 * rgba.g) << argb32_t::k_gshift | uint32_t(255 * rgba.b) << argb32_t::k_bshift};
}

// * Color differences
/**
 * @brief Fast perceptive difference.
 * Adapted from https://www.compuphase.com/cmetric.htm
 *
 * @param col1 First color
 * @param col2 Second color
 * @return float
 */
float delta_E_cmetric(argb32_t col1, argb32_t col2);

/**
 * @brief (Slower) CIE delta E squared, 1976 formula (Lab space Euclidean distance).
 *
 * @param col1 First color
 * @param col2 Second color
 * @return float
 */
float delta_E2_CIE76(ColorCIELab col1, ColorCIELab col2);

/**
 * @brief (Even slower) CIE delta E squared, 1994 formula (L*C*h* distance that addresses perceptual non-uniformities).
 *
 * @param col1 First color
 * @param col2 Second color
 * @return float
 */
float delta_E2_CIE94(ColorCIELab col1, ColorCIELab col2);

} // namespace math
} // namespace kb