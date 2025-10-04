#include "test.h"

#include <base/color.h>

#include <gtest/gtest.h>

TEST(Color, HslToRgbToHslConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorHSLA OldHsl = ColorHSLA(PackedColor);
		ColorRGBA ConvertedRgb = color_cast<ColorRGBA>(OldHsl);
		ColorHSLA NewHsl = color_cast<ColorHSLA>(ConvertedRgb);

		if(OldHsl.s == 0.0f || OldHsl.s == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else if(OldHsl.l == 0.0f || OldHsl.l == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else
		{
			ASSERT_NEAR(std::fmod(OldHsl.h, 1.0f), std::fmod(NewHsl.h, 1.0f), 0.001f);
			ASSERT_NEAR(OldHsl.s, NewHsl.s, 0.0001f);
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
	}
}

TEST(Color, RgbToHslToRgbConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorRGBA OldRgb = ColorRGBA(PackedColor);
		ColorHSLA ConvertedHsl = color_cast<ColorHSLA>(OldRgb);
		ColorRGBA NewRgb = color_cast<ColorRGBA>(ConvertedHsl);

		ASSERT_NEAR(OldRgb.r, NewRgb.r, 0.000001f);
		ASSERT_NEAR(OldRgb.g, NewRgb.g, 0.000001f);
		ASSERT_NEAR(OldRgb.b, NewRgb.b, 0.000001f);
	}
}

TEST(Color, HslToHsvToHslConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorHSLA OldHsl = ColorHSLA(PackedColor);
		ColorHSVA ConvertedHsv = color_cast<ColorHSVA>(OldHsl);
		ColorHSLA NewHsl = color_cast<ColorHSLA>(ConvertedHsv);

		if(OldHsl.s == 0.0f || OldHsl.s == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else if(OldHsl.l == 0.0f || OldHsl.l == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else
		{
			ASSERT_NEAR(std::fmod(OldHsl.h, 1.0f), std::fmod(NewHsl.h, 1.0f), 0.001f);
			ASSERT_NEAR(OldHsl.s, NewHsl.s, 0.0001f);
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
	}
}

TEST(Color, HsvToHslToHsvConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorHSVA OldHsv = ColorHSVA(PackedColor);
		ColorHSLA ConvertedHsl = color_cast<ColorHSLA>(OldHsv);
		ColorHSVA NewHsv = color_cast<ColorHSVA>(ConvertedHsl);

		if(OldHsv.s == 0.0f || OldHsv.s == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsv.v, NewHsv.v);
		}
		else if(OldHsv.v == 0.0f || OldHsv.v == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsv.v, NewHsv.v);
		}
		else
		{
			ASSERT_NEAR(std::fmod(OldHsv.h, 1.0f), std::fmod(NewHsv.h, 1.0f), 0.001f);
			ASSERT_NEAR(OldHsv.s, NewHsv.s, 0.0001f);
			ASSERT_FLOAT_EQ(OldHsv.v, NewHsv.v);
		}
	}
}

// Any color_cast should keep the same alpha value
TEST(Color, ConvKeepsAlpha)
{
	const int Max = 100;
	for(int i = 0; i <= Max; i++)
	{
		const float Alpha = i / (float)Max;
		ASSERT_FLOAT_EQ(color_cast<ColorRGBA>(ColorHSLA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorRGBA>(ColorHSVA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSLA>(ColorRGBA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSLA>(ColorHSVA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSVA>(ColorRGBA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSVA>(ColorHSLA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
	}
}
