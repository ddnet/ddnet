#include "test.h"
#include <gtest/gtest.h>

#include <base/color.h>
#include <base/system.h>

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
			EXPECT_NEAR(fmod(hsl.h, 1.0f), fmod(hsl2.h, 1.0f), 0.001f);
			EXPECT_NEAR(hsl.s, hsl2.s, 0.0001f);
			EXPECT_FLOAT_EQ(hsl.l, hsl2.l);
		}
	}
}
