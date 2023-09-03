#include "test.h"
#include <gtest/gtest.h>

#include <base/bezier.h>

// Due to the implementation, derivatives must be divisible by 3 to be exactly
// represented.

TEST(Bezier, Constant)
{
	CCubicBezier Zero = CCubicBezier::With(0.0f, 0.0f, 0.0f, 0.0f);
	ASSERT_EQ(Zero.Evaluate(0.0f), 0.0f);
	ASSERT_EQ(Zero.Evaluate(0.3f), 0.0f);
	ASSERT_EQ(Zero.Evaluate(1.0f), 0.0f);
	ASSERT_EQ(Zero.Derivative(0.0f), 0.0f);
	ASSERT_EQ(Zero.Derivative(0.3f), 0.0f);
	ASSERT_EQ(Zero.Derivative(1.0f), 0.0f);

	CCubicBezier Five = CCubicBezier::With(5.0f, 0.0f, 0.0f, 5.0f);
	ASSERT_EQ(Five.Evaluate(0.0f), 5.0f);
	ASSERT_EQ(Five.Evaluate(0.3f), 5.0f);
	ASSERT_EQ(Five.Evaluate(1.0f), 5.0f);
	ASSERT_EQ(Five.Derivative(0.0f), 0.0f);
	ASSERT_EQ(Five.Derivative(0.3f), 0.0f);
	ASSERT_EQ(Five.Derivative(1.0f), 0.0f);
}

TEST(Bezier, Linear)
{
	CCubicBezier TripleSlope = CCubicBezier::With(4.0f, 3.0f, 3.0f, 7.0f);
	ASSERT_EQ(TripleSlope.Evaluate(0.0f), 4.0f);
	ASSERT_EQ(TripleSlope.Evaluate(0.3f), 4.9f);
	ASSERT_EQ(TripleSlope.Evaluate(1.0f), 7.0f);
	ASSERT_EQ(TripleSlope.Derivative(0.0f), 3.0f);
	ASSERT_EQ(TripleSlope.Derivative(0.3f), 3.0f);
	ASSERT_EQ(TripleSlope.Derivative(1.0f), 3.0f);
}

TEST(Bezier, Arbitrary)
{
	CCubicBezier Something = CCubicBezier::With(4.9f, 1.5f, -4.5f, 1.0f);
	ASSERT_EQ(Something.Evaluate(0.0f), 4.9f);
	ASSERT_EQ(Something.Evaluate(1.0f), 1.0f);
	ASSERT_EQ(Something.Derivative(0.0f), 1.5f);
	ASSERT_EQ(Something.Derivative(1.0f), -4.5f);
}
