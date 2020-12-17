#pragma once
#include <cstdint>

namespace kb
{
namespace math
{

struct ColorRGBA;
struct ColorHSLA
{
	float h,s,l,a;

	ColorHSLA();
	ColorHSLA(float h, float s, float l, float a = 1.f);
	ColorHSLA(const ColorRGBA&);

	static ColorHSLA random_hue(float s = 1.f, float l = 0.5f, unsigned long long seed = 0);
};

struct ColorRGBA
{
	float r,g,b,a;

	ColorRGBA();
	ColorRGBA(float r, float g, float b, float a = 1.f);
	ColorRGBA(const ColorHSLA&);
};

ColorRGBA to_RGBA(const ColorHSLA&);
ColorHSLA to_HSLA(const ColorRGBA&);

using argb32_t = uint32_t;

constexpr argb32_t pack_ARGB(const ColorRGBA& rgba)
{
	return argb32_t(255*rgba.b) << 0
		 | argb32_t(255*rgba.g) << 8
		 | argb32_t(255*rgba.r) << 16
		 | argb32_t(255*rgba.a) << 24;
}

inline ColorRGBA unpack_ARGB(argb32_t packed_color)
{
	return
	{
		float((packed_color & 0x00ff0000) >> 16) / 255.f,
		float((packed_color & 0x0000ff00) >> 8) / 255.f,
		float((packed_color & 0x000000ff) >> 0) / 255.f,
		float((packed_color & 0xff000000) >> 24) / 255.f
	};
}

} // namespace math
} // namespace kb