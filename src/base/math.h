/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_MATH_H
#define BASE_MATH_H

#include <algorithm>
#include <math.h>
#include <stdlib.h>

using std::clamp;

constexpr inline int round_to_int(float f)
{
	return f > 0 ? (int)(f + 0.5f) : (int)(f - 0.5f);
}

constexpr inline int round_truncate(float f)
{
	return (int)f;
}

inline int round_ceil(float f)
{
	return (int)ceilf(f);
}

template<typename T, typename TB>
constexpr inline T mix(const T a, const T b, TB amount)
{
	return a + (b - a) * amount;
}

inline float random_float()
{
	return rand() / (float)(RAND_MAX);
}

constexpr int fxpscale = 1 << 10;

// float to fixed
constexpr inline int f2fx(float v)
{
	return (int)(v * fxpscale);
}
constexpr inline float fx2f(int v)
{
	return v / (float)fxpscale;
}

// int to fixed
constexpr inline int i2fx(int v)
{
	return v * fxpscale;
}
constexpr inline int fx2i(int v)
{
	return v / fxpscale;
}

class fxp
{
	int value;

public:
	void set(int v)
	{
		value = v;
	}
	int get() const
	{
		return value;
	}
	fxp &operator=(int v)
	{
		value = i2fx(v);
		return *this;
	}
	fxp &operator=(float v)
	{
		value = f2fx(v);
		return *this;
	}
	operator int() const
	{
		return fx2i(value);
	}
	operator float() const
	{
		return fx2f(value);
	}
};

constexpr float pi = 3.1415926535897932384626433f;

template<typename T>
constexpr inline T minimum(T a, T b)
{
	return std::min(a, b);
}
template<typename T>
constexpr inline T minimum(T a, T b, T c)
{
	return std::min(std::min(a, b), c);
}
template<typename T>
constexpr inline T maximum(T a, T b)
{
	return std::max(a, b);
}
template<typename T>
constexpr inline T maximum(T a, T b, T c)
{
	return std::max(std::max(a, b), c);
}
template<typename T>
constexpr inline T absolute(T a)
{
	return a < T(0) ? -a : a;
}

template<typename T>
constexpr inline T in_range(T a, T lower, T upper)
{
	return lower <= a && a <= upper;
}
template<typename T>
constexpr inline T in_range(T a, T upper)
{
	return in_range(a, 0, upper);
}

#endif // BASE_MATH_H
