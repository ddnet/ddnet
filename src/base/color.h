
#ifndef BASE_COLOR_H
#define BASE_COLOR_H

#include "math.h"
#include "vmath.h"

/*
	Title: Color handling
*/
/*
	Function: RgbToHue
		Determines the hue from RGB values
*/
inline float RgbToHue(float r, float g, float b)
{
	float h_min = minimum(r, g, b);
	float h_max = maximum(r, g, b);

	float hue = 0.0f;
	if(h_max != h_min)
	{
		float c = h_max - h_min;
		if(h_max == r)
			hue = (g - b) / c + (g < b ? 6 : 0);
		else if(h_max == g)
			hue = (b - r) / c + 2;
		else
			hue = (r - g) / c + 4;
	}

	return hue / 6.0f;
}

class color4_base
{
public:
	union { float x, r, h; };
	union { float y, g, s; };
	union { float z, b, l, v; };
	union { float w, a; };

	color4_base() {}

	color4_base(const vec4 &v4)
	{
		x = v4.x;
		y = v4.y;
		z = v4.z;
		a = v4.w;
	}

	color4_base(const vec3 &v3)
	{
		x = v3.x;
		y = v3.y;
		z = v3.z;
		a = 1.0f;
	}

	color4_base(float nx, float ny, float nz, float na)
	{
		x = nx;
		y = ny;
		z = nz;
		a = na;
	}

	color4_base(float nx, float ny, float nz)
	{
		x = nx;
		y = ny;
		z = nz;
		a = 1.0f;
	}

	color4_base(unsigned col)
	{
		a = ((col >> 24) & 0xFF) / 255.0f;
		x = ((col >> 16) & 0xFF) / 255.0f;
		y = ((col >> 8) & 0xFF) / 255.0f;
		z = ((col >> 0) & 0xFF) / 255.0f;
	}

	vec4 v4() { return vec4(x, y, z, a); };

	unsigned Pack()
	{
		return ((unsigned)(a * 255.0f) << 24) + ((unsigned)(x * 255.0f) << 16) + ((unsigned)(y * 255.0f) << 8) + (unsigned)(z * 255.0f);
	}

	color4_base SetAlpha(float alpha)
	{
		color4_base col;
		col = *this;
		col.a = alpha;
		return col;
	}

};

class ColorHSLA : public color4_base
{
public:
	using color4_base::color4_base;
	ColorHSLA(color4_base b): color4_base(b) {};

	ColorHSLA Lighten()
	{
		ColorHSLA col = *this;
		col.l = 0.5f + l * 0.5f;
		return col;
	};
};

class ColorHSVA : public color4_base
{
public:
	using color4_base::color4_base;
	ColorHSVA(color4_base b): color4_base(b) {};
};

class ColorRGBA : public color4_base
{
public:
	using color4_base::color4_base;
	ColorRGBA(color4_base b): color4_base(b) {};
};

template <typename T, typename F> T color_cast(const F &f) = delete;

template <>
inline ColorHSLA color_cast(const ColorRGBA &rgb)
{
	float Min = minimum(rgb.r, rgb.g, rgb.b);
	float Max = maximum(rgb.r, rgb.g, rgb.b);

	float c = Max - Min;
	float h = RgbToHue(rgb.r, rgb.g, rgb.b);
	float l = 0.5f * (Max + Min);
	float s = (Max != 0.0f && Min != 1.0f) ? (c/(1 - (absolute(2 * l - 1)))) : 0;

	return ColorHSLA(h, s, l, rgb.a);
}

template <>
inline ColorRGBA color_cast(const ColorHSLA &hsl)
{
	vec3 rgb = vec3(0, 0, 0);

	float h1 = hsl.h * 6;
	float c = (1 - absolute(2 * hsl.l - 1)) * hsl.s;
	float x = c * (1 - absolute(fmod(h1, 2) - 1));

	switch(round_truncate(h1)) {
	case 0:
		rgb.r = c, rgb.g = x;
		break;
	case 1:
		rgb.r = x, rgb.g = c;
		break;
	case 2:
		rgb.g = c, rgb.b = x;
		break;
	case 3:
		rgb.g = x, rgb.b = c;
		break;
	case 4:
		rgb.r = x, rgb.b = c;
		break;
	case 5:
	case 6:
		rgb.r = c, rgb.b = x;
		break;
	}

	float m = hsl.l - (c/2);
	return ColorRGBA(rgb.r + m, rgb.g + m, rgb.b + m, hsl.a);
}

template <>
inline ColorHSLA color_cast(const ColorHSVA &hsv)
{
	float l = hsv.v * (1 - hsv.s * 0.5f);
	return ColorHSLA(hsv.h, (l == 0.0f || l == 1.0f) ? 0 : (hsv.v - l)/minimum(l, 1 - l), l);
}

template <>
inline ColorHSVA color_cast(const ColorHSLA &hsl)
{
	float v = hsl.l + hsl.s * minimum(hsl.l, 1 - hsl.l);
	return ColorHSVA(hsl.h, v == 0.0f ? 0 : 2 - (2 * hsl.l / v), v);
}

template <>
inline ColorRGBA color_cast(const ColorHSVA &hsv)
{
	return color_cast<ColorRGBA>(color_cast<ColorHSLA>(hsv));
}

template <>
inline ColorHSVA color_cast(const ColorRGBA &rgb)
{
	return color_cast<ColorHSVA>(color_cast<ColorHSLA>(rgb));
}

template <typename T>
T color_scale(const color4_base &col, float s)
{
	return T(col.x * s, col.y * s, col.z * s, col.a * s);
}

#endif
