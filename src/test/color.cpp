#include "test.h"
#include <gtest/gtest.h>

#include <base/color.h>

TEST(Color, HRHConv)
{
	for(int i = 0; i < 0xFFFFFF; i += 0xFF)
	{
		ColorHSLA hsl = i;
		ColorRGBA rgb = color_cast<ColorRGBA>(hsl);
		ColorHSLA hsl2 = color_cast<ColorHSLA>(rgb);

		if(hsl.s == 0.0f || hsl.s == 1.0f)
			EXPECT_FLOAT_EQ(hsl.l, hsl2.l);
		else if(hsl.l == 0.0f || hsl.l == 1.0f)
			EXPECT_FLOAT_EQ(hsl.l, hsl2.l);
		else
		{
			EXPECT_NEAR(std::fmod(hsl.h, 1.0f), std::fmod(hsl2.h, 1.0f), 0.001f);
			EXPECT_NEAR(hsl.s, hsl2.s, 0.0001f);
			EXPECT_FLOAT_EQ(hsl.l, hsl2.l);
		}
	}
}

// Any color_cast should keep the same alpha value
TEST(Color, ConvKeepsAlpha)
{
	const int Max = 10;
	for(int i = 0; i <= Max; i++)
	{
		const float Alpha = i / (float)Max;
		EXPECT_FLOAT_EQ(color_cast<ColorRGBA>(ColorHSLA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		EXPECT_FLOAT_EQ(color_cast<ColorRGBA>(ColorHSVA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		EXPECT_FLOAT_EQ(color_cast<ColorHSLA>(ColorRGBA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		EXPECT_FLOAT_EQ(color_cast<ColorHSLA>(ColorHSVA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		EXPECT_FLOAT_EQ(color_cast<ColorHSVA>(ColorRGBA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		EXPECT_FLOAT_EQ(color_cast<ColorHSVA>(ColorHSLA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
	}
}
