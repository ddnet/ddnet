/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_MATH_H
#define BASE_MATH_H

#include <math.h>
#include <stdlib.h>

template<typename T>
constexpr inline T clamp(T val, T min, T max)
{
	return val < min ? min : (val > max ? max : val);
}

constexpr inline float sign(float f)
{
	return f < 0.0f ? -1.0f : 1.0f;
}

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

inline float frandom() { return rand() / (float)(RAND_MAX); }

// float to fixed
constexpr inline int f2fx(float v) { return (int)(v * (float)(1 << 10)); }
constexpr inline float fx2f(int v) { return v * (1.0f / (1 << 10)); }

// int to fixed
inline int i2fx(int v)
{
	return v << 10;
}
inline int fx2i(int v)
{
	return v >> 10;
}

inline int gcd(int a, int b)
{
	while(b != 0)
	{
		int c = a % b;
		a = b;
		b = c;
	}
	return a;
}

class fxp
{
	int value;

public:
	void set(int v) { value = v; }
	int get() const { return value; }
	fxp &operator=(int v)
	{
		value = v << 10;
		return *this;
	}
	fxp &operator=(float v)
	{
		value = (int)(v * (float)(1 << 10));
		return *this;
	}
	operator float() const { return value / (float)(1 << 10); }
};

constexpr float pi = 3.1415926535897932384626433f;

template<typename T>
constexpr inline T minimum(T a, T b)
{
	return a < b ? a : b;
}
template<typename T>
constexpr inline T minimum(T a, T b, T c)
{
	return minimum(minimum(a, b), c);
}
template<typename T>
constexpr inline T maximum(T a, T b)
{
	return a > b ? a : b;
}
template<typename T>
constexpr inline T maximum(T a, T b, T c)
{
	return maximum(maximum(a, b), c);
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
