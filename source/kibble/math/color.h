#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kb::math
{

// * Color formats

struct ColorRGBA;
struct ColorHSLA;
struct ColorCIELab;

/**
 * @brief Holds an ARGB color packed inside a 32b value
 *
 */
struct argb32_t
{
    static constexpr inline uint32_t k_amask = 0xff000000u;
    static constexpr inline uint32_t k_rmask = 0x00ff0000u;
    static constexpr inline uint32_t k_gmask = 0x0000ff00u;
    static constexpr inline uint32_t k_bmask = 0x000000ffu;
    static constexpr inline uint32_t k_ashift = 24u;
    static constexpr inline uint32_t k_rshift = 16u;
    static constexpr inline uint32_t k_gshift = 8u;
    static constexpr inline uint32_t k_bshift = 0u;

    uint32_t value{0u};

    // clang-format off
    /// @brief Return the value of the alpha channel
    constexpr inline uint32_t a() const { return (value & argb32_t::k_amask) >> argb32_t::k_ashift; }
    /// @brief Return the value of the red channel
    constexpr inline uint32_t r() const { return (value & argb32_t::k_rmask) >> argb32_t::k_rshift; }
    /// @brief Return the value of the green channel
    constexpr inline uint32_t g() const { return (value & argb32_t::k_gmask) >> argb32_t::k_gshift; }
    /// @brief Return the value of the blue channel
    constexpr inline uint32_t b() const { return (value & argb32_t::k_bmask) >> argb32_t::k_bshift; }
    /// @brief Access a color channel by index
    constexpr inline uint32_t operator[](int chan) const     { return (value & (uint32_t(0xff) << (chan * 8))) >> (chan * 8); }
    /// @brief Directly assign a 32b value
    constexpr inline const argb32_t& operator=(uint32_t val) { value = val; return *this; }
    /// @brief Value comparison
    constexpr inline bool operator==(const argb32_t& other)  { return value == other.value; }
    // clang-format on

    /// @brief Build from channels
    static constexpr inline argb32_t pack(uint32_t R, uint32_t G, uint32_t B, uint32_t A = 255)
    {
        return {((R & 0xff) << k_rshift) | ((G & 0xff) << k_gshift) | ((B & 0xff) << k_bshift) |
                ((A & 0xff) << k_ashift)};
    }

    /// @brief Convert to 32b unsigned ABGR format
    constexpr inline uint32_t to_abgr32() const
    {
        return (value & k_amask) | ((value & k_bmask) << 16) | (value & k_gmask) | ((value & k_rmask) >> 16);
    }

    /// @brief Convert from 32b unsigned ABGR format
    static constexpr inline argb32_t from_abgr32(uint32_t color)
    {
        return {(color & k_amask) | ((color & 0x00ff0000) >> 16) | (color & 0x0000ff00) | ((color & 0x000000ff) << 16)};
    }

    /// @brief Convert to normalized floating point vector
    template <typename Vec4T>
    constexpr inline Vec4T to_rgba_vec() const
    {
        return {float(r()) / 255.f, float(g()) / 255.f, float(b()) / 255.f, float(a()) / 255.f};
    }

    /// @brief Convert to normalized floating point vector
    template <typename Vec3T>
    constexpr inline Vec3T to_rgb_vec() const
    {
        return {float(r()) / 255.f, float(g()) / 255.f, float(b()) / 255.f};
    }

    /// @brief Convert from normalized floating point vector
    template <typename Vec4T>
    static constexpr inline argb32_t from_rgba_vec(const Vec4T& color)
    {
        return {(uint32_t(std::roundf(std::clamp(color[0], 0.f, 1.f) * 255.f)) << k_rshift) |
                (uint32_t(std::roundf(std::clamp(color[1], 0.f, 1.f) * 255.f)) << k_gshift) |
                (uint32_t(std::roundf(std::clamp(color[2], 0.f, 1.f) * 255.f)) << k_bshift) |
                (uint32_t(std::roundf(std::clamp(color[3], 0.f, 1.f) * 255.f)) << k_ashift)};
    }

    /// @brief Convert from normalized floating point vector
    template <typename Vec3T>
    static constexpr inline argb32_t from_rgb_vec(const Vec3T& color)
    {
        return {(uint32_t(std::roundf(std::clamp(color[0], 0.f, 1.f) * 255.f)) << k_rshift) |
                (uint32_t(std::roundf(std::clamp(color[1], 0.f, 1.f) * 255.f)) << k_gshift) |
                (uint32_t(std::roundf(std::clamp(color[2], 0.f, 1.f) * 255.f)) << k_bshift) |
                (uint32_t(0xff) << k_ashift)};
    }

    /// @brief Convert to kibble normalized floating point RGBA format
    ColorRGBA to_rgba() const;
};

/**
 * @brief Represents a color in the RGBA color space.
 * Each channel is floating point.
 *
 */
struct ColorRGBA
{
    float r{0.f};
    float g{0.f};
    float b{0.f};
    float a{0.f};

    template <typename Vec4T>
    static constexpr inline ColorRGBA from_rgba_vec(const Vec4T& color)
    {
        return {.r = color[0], .g = color[1], .b = color[2], .a = color[3]};
    }

    /// @brief Convert to kibble normalized floating point HSLA format
    ColorHSLA to_hsla() const;
    /// @brief Convert a color from sRGBA space to CIELab.
    ColorCIELab to_CIELab() const;
    /// @brief Convert to kibble argb32_t format
    argb32_t to_argb32() const;
};

/**
 * @brief Represents a color in the HSLA color space.
 * Each channel is floating point.
 *
 */
struct ColorHSLA
{
    float h{0.f};
    float s{0.f};
    float l{0.f};
    float a{0.f};

    /// @brief Convert to kibble normalized floating point RGBA format
    ColorRGBA to_rgba() const;
};

/**
 * @brief Represents a color in the CIELab color space.
 * Each channel is floating point.
 *
 */
struct ColorCIELab
{
    float L{0.f};
    float a{0.f};
    float b{0.f};

    /// @brief Convert to kibble normalized floating point RGBA format
    ColorRGBA to_rgba() const;
};

// * Color transformations

/**
 * @internal
 * @brief Linearly interpolates between two packed ARGB colors, channel by channel.
 *
 * @param col1 Start color (t = 0)
 * @param col2 End color (t = 1)
 * @param t Blend factor, clamped to [0, 1]
 */
argb32_t lerp(argb32_t col1, argb32_t col2, float t);

/**
 * @brief Scale alpha channel of a packed ARGB color
 *
 * @param color Initial color
 * @param alpha Alpha multiplier
 * @return argb32_t
 */
argb32_t modulate_alpha(argb32_t color, float alpha);

/**
 * @brief Transform a color by multiplication of each color channel by a given factor.
 * Allows to lighten or darken colors. Stays in RGB color space.
 *
 * @param color Input color
 * @param factor Multiplicative factor, clamped between 0 and 1
 * @return argb32_t
 */
argb32_t lighten_darken_rgb_fast(argb32_t color, float factor);

/**
 * @brief Lighten / darken a color, staying in RGB space, preserves hue
 *
 * @param color Initial color
 * @param factor < 1 to darken, > 1 to lighten
 * @return argb32_t
 */
argb32_t lighten_darken_rgb(argb32_t color, float factor);

/**
 * @brief Lighten / darken a color.
 * Uses a roundtrip conversion to HSL
 *
 * @param color Initial color
 * @param factor < 1 to darken, > 1 to lighten
 * @return argb32_t
 */
argb32_t lighten_darken(argb32_t color, float factor);

/**
 * @brief Create an HSLA color of random hue.
 *
 * @param s Saturation
 * @param l Lightness
 * @param seed Seed for the RNG
 * @return ColorHSLA
 */
ColorHSLA random_hue(float s = 1.f, float l = 0.5f, unsigned long long seed = 0);

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

} // namespace kb::math