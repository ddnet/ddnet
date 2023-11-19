#include "test.h"
#include <gtest/gtest.h>

#include <base/math.h>

TEST(Math, FixedPointRoundtrip)
{
	for(int i = 0; i < 100000; ++i)
	{
		const float Number = i / 1000.0f;
		EXPECT_NEAR(Number, fx2f(f2fx(Number)), 0.0005f);
		EXPECT_EQ(i, f2fx(fx2f(i)));
	}
}
