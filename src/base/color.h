
#ifndef BASE_COLOR_H
#define BASE_COLOR_H

#include "math.h"
#include "vmath.h"

/*
	Title: Color handling
*/

/*
	Function: HueToRgb
		Converts Hue to RGB
*/
inline float HueToRgb(float v1, float v2, float h)
{
	if(h < 0.0f) h += 1;
	if(h > 1.0f) h -= 1;
	if((6.0f * h) < 1.0f) return v1 + (v2 - v1) * 6.0f * h;
	if((2.0f * h) < 1.0f) return v2;
	if((3.0f * h) < 2.0f) return v1 + (v2 - v1) * ((2.0f/3.0f) - h) * 6.0f;
	return v1;
}

inline float RgbToHue(vec3 rgb)
{
	float h_min = min(rgb.r, rgb.g, rgb.b);
	float h_max = max(rgb.r, rgb.g, rgb.b);

	float hue = 0.0f;
	if(h_max != h_min)
	{
		float c = h_max - h_min;
		if(h_max == rgb.r)
			hue = (rgb.g - rgb.b) / c + (rgb.g < rgb.b ? 6 : 0);
		else if(h_max == rgb.g)
			hue = (rgb.b - rgb.r) / c + 2;
		else
			hue = (rgb.r - rgb.g) / c + 4;
	}

	return hue / 6.0f;
}

/*
	Function: HslToRgb
		Converts HSL to RGB
*/
inline vec3 HslToRgb(vec3 HSL)
{
	vec3 rgb = vec3(0, 0, 0);

	float h1 = HSL.h * 6;
	float c = (1 - absolute(2 * HSL.l - 1)) * HSL.s;
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
		rgb.r = c, rgb.b = x;
		break;
	}

	float m = HSL.l - (c/2);
	return vec3(rgb.r + m, rgb.g + m, rgb.b + m);
}

inline vec3 HsvToRgb(vec3 hsv)
{
	int h = int(hsv.x * 6.0f);
	float f = hsv.x * 6.0f - h;
	float p = hsv.z * (1.0f - hsv.y);
	float q = hsv.z * (1.0f - hsv.y * f);
	float t = hsv.z * (1.0f - hsv.y * (1.0f - f));

	vec3 rgb = vec3(0.0f, 0.0f, 0.0f);

	switch(h % 6)
	{
	case 0:
		rgb.r = hsv.z;
		rgb.g = t;
		rgb.b = p;
		break;

	case 1:
		rgb.r = q;
		rgb.g = hsv.z;
		rgb.b = p;
		break;

	case 2:
		rgb.r = p;
		rgb.g = hsv.z;
		rgb.b = t;
		break;

	case 3:
		rgb.r = p;
		rgb.g = q;
		rgb.b = hsv.z;
		break;

	case 4:
		rgb.r = t;
		rgb.g = p;
		rgb.b = hsv.z;
		break;

	case 5:
		rgb.r = hsv.z;
		rgb.g = p;
		rgb.b = q;
		break;
	}

	return rgb;
}

inline vec3 RgbToHsv(vec3 rgb)
{
	float h_min = min(min(rgb.r, rgb.g), rgb.b);
	float h_max = max(max(rgb.r, rgb.g), rgb.b);

	// hue
	float hue = 0.0f;

	if(h_max == h_min)
		hue = 0.0f;
	else if(h_max == rgb.r)
		hue = (rgb.g-rgb.b) / (h_max-h_min);
	else if(h_max == rgb.g)
		hue = 2.0f + (rgb.b-rgb.r) / (h_max-h_min);
	else
		hue = 4.0f + (rgb.r-rgb.g) / (h_max-h_min);

	hue /= 6.0f;

	if(hue < 0.0f)
		hue += 1.0f;

	// saturation
	float s = 0.0f;
	if(h_max != 0.0f)
		s = (h_max - h_min)/h_max;

	// lightness
	float l = h_max;

	return vec3(hue, s, l);
}

inline vec3 RgbToHsl(vec3 rgb)
{
	float Min = min(rgb.r, rgb.g, rgb.b);
	float Max = max(rgb.r, rgb.g, rgb.b);

	float c = Max - Min;
	float h = RgbToHue(rgb);
	float l = 0.5f * (Max + Min);
	float s = (Max != 0.0f && Min != 1.0f) ? (c/(1 - (absolute(2 * l - 1)))) : 0;

	return vec3(h, s, l);
}

/*
	Function: HexToRgba
		Converts Hex to Rgba

	Remarks: Hex should be RGBA8
*/
inline vec4 HexToRgba(int hex)
{
	vec4 c;
	c.r = ((hex >> 24) & 0xFF) / 255.0f;
	c.g = ((hex >> 16) & 0xFF) / 255.0f;
	c.b = ((hex >> 8) & 0xFF) / 255.0f;
	c.a = (hex & 0xFF) / 255.0f;

	return c;
}

inline vec3 UnpackColor(int v)
{
    return vec3(fmod(((v>>16)&0xff)/255.0f, 1.0f), ((v>>8)&0xff)/255.0f, 0.5f+(v&0xff)/255.0f*0.5f);
}

inline int PackColor(vec3 hsl)
{
	return ((int)(hsl.h * 255.0f) << 16) + ((int)(hsl.s * 255.0f) << 8) + (int)(hsl.l * 255.0f);
}

inline vec4 Color3to4(vec3 col)
{
	return vec4(col[0], col[1], col[2], 1.0f);
}

#endif
