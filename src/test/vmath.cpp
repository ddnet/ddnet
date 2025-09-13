#include "test.h"
#include <gtest/gtest.h>

#include <base/vmath.h>

TEST(VMath, DistanceVec2)
{
	vec2 Pos1(0, 0);
	vec2 Pos2(30000, 40000);
	float Distance = distance(Pos1, Pos2);
	EXPECT_NEAR(Distance, 50000, 0.0005f);
}

TEST(VMath, DistanceVec3)
{
	vec3 Pos1(0, 0, 0);
	vec3 Pos2(30000, 0, 40000);
	float Distance = distance(Pos1, Pos2);
	EXPECT_NEAR(Distance, 50000, 0.0005f);
}

TEST(VMath, DistanceIVec2)
{
	ivec2 Pos1(0, 0);
	ivec2 Pos2(30000, 40000);
	int Distance = distance(Pos1, Pos2);
	EXPECT_NEAR(Distance, 50000, 0.0005f);
}

TEST(VMath, DistanceIVec3)
{
	ivec3 Pos1(0, 0, 0);
	ivec3 Pos2(30000, 0, 40000);
	int Distance = distance(Pos1, Pos2);
	EXPECT_NEAR(Distance, 50000, 0.0005f);
}
