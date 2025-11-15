#include "test.h"

#include <base/fixed.h>

#include <gtest/gtest.h>

TEST(Fixed, FromLiteral)
{
	// Basic integers
	constexpr CFixed Zero = CFixed::FromLiteral("0");
	constexpr CFixed One = CFixed::FromLiteral("1");
	constexpr CFixed Ten = CFixed::FromLiteral("10");
	constexpr CFixed Hundred = CFixed::FromLiteral("100");
	constexpr CFixed Large = CFixed::FromLiteral("12345");

	EXPECT_TRUE(Zero.IsZero());
	EXPECT_FALSE(One.IsZero());
	EXPECT_EQ(Ten, CFixed::FromInt(10));
	EXPECT_EQ(Hundred, CFixed::FromInt(100));
	EXPECT_EQ(Large, CFixed::FromInt(12345));

	// Negative integers
	constexpr CFixed NegOne = CFixed::FromLiteral("-1");
	constexpr CFixed NegTen = CFixed::FromLiteral("-10");

	EXPECT_EQ(NegOne, CFixed::FromInt(-1));
	EXPECT_EQ(NegTen, CFixed::FromInt(-10));

	// Decimals
	constexpr CFixed OnePointFive = CFixed::FromLiteral("1.5");
	constexpr CFixed Pi = CFixed::FromLiteral("3.141");
	constexpr CFixed Small = CFixed::FromLiteral("0.001");

	EXPECT_NEAR(1.5f, static_cast<float>(OnePointFive), 0.001f);
	EXPECT_NEAR(3.141f, static_cast<float>(Pi), 0.001f);
	EXPECT_NEAR(0.001f, static_cast<float>(Small), 0.0001f);

	// Negative decimals
	constexpr CFixed NegPi = CFixed::FromLiteral("-3.141");

	EXPECT_NEAR(-3.141f, static_cast<float>(NegPi), 0.001f);

	// Trailing zeros
	constexpr CFixed TwoPointZero = CFixed::FromLiteral("2.0");
	constexpr CFixed ThreePointZeroZero = CFixed::FromLiteral("3.00");

	EXPECT_EQ(TwoPointZero, CFixed::FromInt(2));
	EXPECT_EQ(ThreePointZeroZero, CFixed::FromInt(3));

	// Leading zeros in fraction
	constexpr CFixed PointZeroOne = CFixed::FromLiteral("0.01");
	constexpr CFixed PointZeroZeroOne = CFixed::FromLiteral("0.001");

	EXPECT_NEAR(0.01f, static_cast<float>(PointZeroOne), 0.0001f);
	EXPECT_NEAR(0.001f, static_cast<float>(PointZeroZeroOne), 0.0001f);

	// Trailing dot
	constexpr CFixed FiveDot = CFixed::FromLiteral("5.");

	EXPECT_EQ(FiveDot, CFixed::FromInt(5));

	// Plus sign
	constexpr CFixed PlusOne = CFixed::FromLiteral("+1");
	constexpr CFixed PlusOnePointFive = CFixed::FromLiteral("+1.5");

	EXPECT_EQ(PlusOne, CFixed::FromInt(1));
	EXPECT_NEAR(1.5f, static_cast<float>(PlusOnePointFive), 0.001f);
}

TEST(Fixed, FromStr)
{
	CFixed Result;

	// Basic integers
	EXPECT_TRUE(CFixed::FromStr("0", Result));
	EXPECT_TRUE(Result.IsZero());

	EXPECT_TRUE(CFixed::FromStr("1", Result));
	EXPECT_FALSE(Result.IsZero());

	EXPECT_TRUE(CFixed::FromStr("12", Result));
	EXPECT_TRUE(CFixed::FromStr("123", Result));
	EXPECT_TRUE(CFixed::FromStr("12345", Result));

	// Negative integers
	EXPECT_TRUE(CFixed::FromStr("-1", Result));
	EXPECT_TRUE(CFixed::FromStr("-123", Result));

	// Decimals
	EXPECT_TRUE(CFixed::FromStr("1.5", Result));
	EXPECT_TRUE(CFixed::FromStr("3.141", Result));
	EXPECT_TRUE(CFixed::FromStr("0.001", Result));

	// Negative decimals
	EXPECT_TRUE(CFixed::FromStr("-3.141", Result));
	EXPECT_TRUE(CFixed::FromStr("-0.5", Result));

	// Edge cases
	EXPECT_TRUE(CFixed::FromStr("0.0", Result));
	EXPECT_TRUE(CFixed::FromStr("5.", Result));
	EXPECT_TRUE(CFixed::FromStr("+1", Result));
	EXPECT_TRUE(CFixed::FromStr("+1.5", Result));

	// Invalid inputs
	EXPECT_FALSE(CFixed::FromStr("", Result));
	EXPECT_FALSE(CFixed::FromStr("abc", Result));
	EXPECT_FALSE(CFixed::FromStr("1.2.3", Result));
	EXPECT_FALSE(CFixed::FromStr("1 2", Result));
	EXPECT_FALSE(CFixed::FromStr(" 1", Result));
	EXPECT_FALSE(CFixed::FromStr("1 ", Result));
	EXPECT_FALSE(CFixed::FromStr("+", Result));
	EXPECT_FALSE(CFixed::FromStr("-", Result));
	EXPECT_FALSE(CFixed::FromStr(".", Result));
}

TEST(Fixed, FromInt)
{
	constexpr CFixed Zero = CFixed::FromInt(0);
	constexpr CFixed One = CFixed::FromInt(1);
	constexpr CFixed Ten = CFixed::FromInt(10);
	constexpr CFixed NegOne = CFixed::FromInt(-1);
	constexpr CFixed Large = CFixed::FromInt(12345);

	EXPECT_TRUE(Zero.IsZero());
	EXPECT_FALSE(One.IsZero());
	EXPECT_EQ(static_cast<float>(Ten), 10.0f);
	EXPECT_EQ(static_cast<float>(NegOne), -1.0f);
	EXPECT_EQ(static_cast<float>(Large), 12345.0f);
}

TEST(Fixed, AsStr)
{
	char Buffer[32];

	// Integers
	CFixed::FromInt(0).AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("0", Buffer);

	CFixed::FromInt(1).AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("1", Buffer);

	CFixed::FromInt(12).AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("12", Buffer);

	CFixed::FromInt(-5).AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("-5", Buffer);

	// Decimals
	CFixed Value;
	CFixed::FromStr("1.5", Value);
	Value.AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("1.5", Buffer);

	CFixed::FromStr("3.141", Value);
	Value.AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("3.141", Buffer);

	CFixed::FromStr("0.001", Value);
	Value.AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("0.001", Buffer);

	// Trailing zeros should be trimmed
	CFixed::FromStr("2.0", Value);
	Value.AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("2", Buffer);

	CFixed::FromStr("3.100", Value);
	Value.AsStr(Buffer, sizeof(Buffer));
	EXPECT_STREQ("3.1", Buffer);
}

TEST(Fixed, Comparisons)
{
	constexpr CFixed Zero = CFixed::FromInt(0);
	constexpr CFixed One = CFixed::FromInt(1);
	constexpr CFixed Two = CFixed::FromInt(2);
	constexpr CFixed NegOne = CFixed::FromInt(-1);

	// Equality
	EXPECT_TRUE(Zero == Zero);
	EXPECT_TRUE(One == One);
	EXPECT_FALSE(Zero == One);

	// Inequality
	EXPECT_TRUE(Zero != One);
	EXPECT_FALSE(One != One);

	// Less than
	EXPECT_TRUE(Zero < One);
	EXPECT_TRUE(NegOne < Zero);
	EXPECT_FALSE(One < Zero);
	EXPECT_FALSE(One < One);

	// Less than or equal
	EXPECT_TRUE(Zero <= One);
	EXPECT_TRUE(One <= One);
	EXPECT_FALSE(Two <= One);

	// Greater than
	EXPECT_TRUE(One > Zero);
	EXPECT_TRUE(Zero > NegOne);
	EXPECT_FALSE(Zero > One);
	EXPECT_FALSE(One > One);

	// Greater than or equal
	EXPECT_TRUE(One >= Zero);
	EXPECT_TRUE(One >= One);
	EXPECT_FALSE(Zero >= One);
}

TEST(Fixed, FloatConversion)
{
	CFixed Value;

	CFixed::FromStr("1.5", Value);
	EXPECT_NEAR(1.5f, static_cast<float>(Value), 0.001f);

	CFixed::FromStr("3.141", Value);
	EXPECT_NEAR(3.141f, static_cast<float>(Value), 0.001f);

	CFixed::FromStr("-2.5", Value);
	EXPECT_NEAR(-2.5f, static_cast<float>(Value), 0.001f);
}
