
#ifndef BASE_COLOR_H
#define BASE_COLOR_H

#include <base/math.h>
#include <base/vmath.h>

#include <optional>

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

// Curiously Recurring Template Pattern for type safety
template<typename DerivedT>
class color4_base
{
public:
	union
	{
		float x, r, h;
	};
	union
	{
		float y, g, s;
	};
	union
	{
		float z, b, l, v;
	};
	union
	{
		float w, a;
	};

	color4_base() :
		x(), y(), z(), a()
	{
	}

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

	color4_base(unsigned col, bool alpha = false)
	{
		a = alpha ? ((col >> 24) & 0xFF) / 255.0f : 1.0f;
		x = ((col >> 16) & 0xFF) / 255.0f;
		y = ((col >> 8) & 0xFF) / 255.0f;
		z = ((col >> 0) & 0xFF) / 255.0f;
	}

	vec4 v4() const { return vec4(x, y, z, a); }
	operator vec4() const { return vec4(x, y, z, a); }
	float &operator[](int index)
	{
		return ((float *)this)[index];
	}

	bool operator==(const color4_base &col) const { return x == col.x && y == col.y && z == col.z && a == col.a; }
	bool operator!=(const color4_base &col) const { return x != col.x || y != col.y || z != col.z || a != col.a; }

	unsigned Pack(bool Alpha = true) const
	{
		return (Alpha ? ((unsigned)round_to_int(a * 255.0f) << 24) : 0) + ((unsigned)round_to_int(x * 255.0f) << 16) + ((unsigned)round_to_int(y * 255.0f) << 8) + (unsigned)round_to_int(z * 255.0f);
	}

	unsigned PackAlphaLast(bool Alpha = true) const
	{
		if(Alpha)
			return ((unsigned)round_to_int(x * 255.0f) << 24) + ((unsigned)round_to_int(y * 255.0f) << 16) + ((unsigned)round_to_int(z * 255.0f) << 8) + (unsigned)round_to_int(a * 255.0f);
		return ((unsigned)round_to_int(x * 255.0f) << 16) + ((unsigned)round_to_int(y * 255.0f) << 8) + (unsigned)round_to_int(z * 255.0f);
	}

	DerivedT WithAlpha(float alpha) const
	{
		DerivedT col(static_cast<const DerivedT &>(*this));
		col.a = alpha;
		return col;
	}

	DerivedT WithMultipliedAlpha(float alpha) const
	{
		DerivedT col(static_cast<const DerivedT &>(*this));
		col.a *= alpha;
		return col;
	}

	DerivedT Multiply(const DerivedT &Other) const
	{
		DerivedT Color(static_cast<const DerivedT &>(*this));
		Color.x *= Other.x;
		Color.y *= Other.y;
		Color.z *= Other.z;
		Color.a *= Other.a;
		return Color;
	}

	template<typename UnpackT>
	static UnpackT UnpackAlphaLast(unsigned Color, bool Alpha = true)
	{
		UnpackT Result;
		if(Alpha)
		{
			Result.x = ((Color >> 24) & 0xFF) / 255.0f;
			Result.y = ((Color >> 16) & 0xFF) / 255.0f;
			Result.z = ((Color >> 8) & 0xFF) / 255.0f;
			Result.a = ((Color >> 0) & 0xFF) / 255.0f;
		}
		else
		{
			Result.x = ((Color >> 16) & 0xFF) / 255.0f;
			Result.y = ((Color >> 8) & 0xFF) / 255.0f;
			Result.z = ((Color >> 0) & 0xFF) / 255.0f;
			Result.a = 1.0f;
		}
		return Result;
	}
};

class ColorHSLA : public color4_base<ColorHSLA>
{
public:
	using color4_base::color4_base;
	ColorHSLA(){};

	constexpr static const float DARKEST_LGT = 0.5f;
	constexpr static const float DARKEST_LGT7 = 61.0f / 255.0f;

	ColorHSLA UnclampLighting(float Darkest) const
	{
		ColorHSLA col = *this;
		col.l = Darkest + col.l * (1.0f - Darkest);
		return col;
	}

	unsigned Pack(bool Alpha = true) const
	{
		return color4_base::Pack(Alpha);
	}

	unsigned Pack(float Darkest, bool Alpha = false) const
	{
		ColorHSLA col = *this;
		col.l = (l - Darkest) / (1 - Darkest);
		col.l = clamp(col.l, 0.0f, 1.0f);
		return col.Pack(Alpha);
	}
};

class ColorHSVA : public color4_base<ColorHSVA>
{
public:
	using color4_base::color4_base;
	ColorHSVA(){};
};

class ColorRGBA : public color4_base<ColorRGBA>
{
public:
	using color4_base::color4_base;
	ColorRGBA(){};
};

template<typename T, typename F>
T color_cast(const F &) = delete;

template<>
inline ColorHSLA color_cast(const ColorRGBA &rgb)
{
	float Min = minimum(rgb.r, rgb.g, rgb.b);
	float Max = maximum(rgb.r, rgb.g, rgb.b);

	float c = Max - Min;
	float h = RgbToHue(rgb.r, rgb.g, rgb.b);
	float l = 0.5f * (Max + Min);
	float s = (Max != 0.0f && Min != 1.0f) ? (c / (1 - (absolute(2 * l - 1)))) : 0;

	return ColorHSLA(h, s, l, rgb.a);
}

template<>
inline ColorRGBA color_cast(const ColorHSLA &hsl)
{
	vec3 rgb = vec3(0, 0, 0);

	float h1 = hsl.h * 6;
	float c = (1.f - absolute(2 * hsl.l - 1)) * hsl.s;
	float x = c * (1.f - absolute(std::fmod(h1, 2) - 1.f));

	switch(round_truncate(h1))
	{
	case 0:
		rgb.r = c;
		rgb.g = x;
		break;
	case 1:
		rgb.r = x;
		rgb.g = c;
		break;
	case 2:
		rgb.g = c;
		rgb.b = x;
		break;
	case 3:
		rgb.g = x;
		rgb.b = c;
		break;
	case 4:
		rgb.r = x;
		rgb.b = c;
		break;
	case 5:
	case 6:
		rgb.r = c;
		rgb.b = x;
		break;
	}

	float m = hsl.l - (c / 2);
	return ColorRGBA(rgb.r + m, rgb.g + m, rgb.b + m, hsl.a);
}

template<>
inline ColorHSLA color_cast(const ColorHSVA &hsv)
{
	float l = hsv.v * (1 - hsv.s * 0.5f);
	return ColorHSLA(hsv.h, (l == 0.0f || l == 1.0f) ? 0 : (hsv.v - l) / minimum(l, 1 - l), l, hsv.a);
}

template<>
inline ColorHSVA color_cast(const ColorHSLA &hsl)
{
	float v = hsl.l + hsl.s * minimum(hsl.l, 1 - hsl.l);
	return ColorHSVA(hsl.h, v == 0.0f ? 0 : 2 - (2 * hsl.l / v), v, hsl.a);
}

template<>
inline ColorRGBA color_cast(const ColorHSVA &hsv)
{
	return color_cast<ColorRGBA>(color_cast<ColorHSLA>(hsv));
}

template<>
inline ColorHSVA color_cast(const ColorRGBA &rgb)
{
	return color_cast<ColorHSVA>(color_cast<ColorHSLA>(rgb));
}

template<typename T>
T color_scale(const T &col, float s)
{
	return T(col.x * s, col.y * s, col.z * s, col.a * s);
}

template<typename T>
T color_invert(const T &col)
{
	return T(1.0f - col.x, 1.0f - col.y, 1.0f - col.z, 1.0f - col.a);
}

template<typename T>
std::optional<T> color_parse(const char *pStr);

#endif
