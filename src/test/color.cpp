#include "test.h"
#include <gtest/gtest.h>

#include <iostream>
#include <base/system.h>
#include <base/color.h>

TEST(Color, HRHConv)
{
    for(int i = 0; i < 0xFFFFFF; i++){
        vec3 hsl = UnpackColor(i);
        vec3 rgb = HslToRgb(hsl);
        vec3 hsl2 = RgbToHsl(rgb);

        if(hsl.s == 0.0f || hsl.s == 1.0f)
            EXPECT_FLOAT_EQ(hsl.l, hsl2.l);
        else if(hsl.l == 1.0f)
            SUCCEED();
        else {
            EXPECT_NEAR(hsl.h, hsl2.h, 0.001f);
            EXPECT_NEAR(hsl.s, hsl2.s, 0.0001f);
            EXPECT_FLOAT_EQ(hsl.l, hsl2.l);
        }
    }
}
