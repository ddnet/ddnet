#include "test.h"

#include <base/vmath.h>

#include <gtest/gtest.h>

TEST(VMath, IntersectLineCircleMiss)
{
	vec2 Start(0, 0);
	vec2 End(0, 1);
	vec2 Circle(2, 0);
	float Radius = 1.0f;

	vec2 aIntersections[2];
	int Size = intersect_line_circle(Start, End, Circle, Radius, aIntersections);
	EXPECT_EQ(Size, 0);
}

TEST(VMath, IntersectLineCircleTangent)
{
	vec2 Start(0, 0);
	vec2 End(0, 1);
	vec2 Circle(1, 0);
	float Radius = 1.0f;

	vec2 aIntersections[2];
	int Size = intersect_line_circle(Start, End, Circle, Radius, aIntersections);
	EXPECT_EQ(Size, 1);
	EXPECT_EQ(aIntersections[0], Start);
}

TEST(VMath, IntersectLineCircleMultipleIntersections)
{
	vec2 Start(0, 0);
	vec2 End(0, 1);
	vec2 Circle(0, 0);
	float Radius = 1.0f;

	vec2 aIntersections[2];
	int Size = intersect_line_circle(Start, End, Circle, Radius, aIntersections);
	EXPECT_EQ(Size, 2);
	EXPECT_EQ(aIntersections[0], -End);
	EXPECT_EQ(aIntersections[1], End);
}

TEST(VMath, IntersectLineCircleMultipleIntersectionsOutsideOfLineSegment)
{
	vec2 Start(1, 0);
	vec2 End(1, 1);
	vec2 Circle(1, 10);
	float Radius = 1.0f;

	vec2 aIntersections[2];
	int Size = intersect_line_circle(Start, End, Circle, Radius, aIntersections);
	EXPECT_EQ(Size, 2);
	EXPECT_EQ(aIntersections[0], vec2(1, 9));
	EXPECT_EQ(aIntersections[1], vec2(1, 11));

	vec2 Circle2(1, -10);
	int Size2 = intersect_line_circle(Start, End, Circle2, Radius, aIntersections);
	EXPECT_EQ(Size2, 2);
	EXPECT_EQ(aIntersections[0], vec2(1, -11));
	EXPECT_EQ(aIntersections[1], vec2(1, -9));
}
