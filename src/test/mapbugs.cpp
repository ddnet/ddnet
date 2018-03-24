#include <gtest/gtest.h>

#include <game/mapbugs.h>

TEST(MapBugs, Contains)
{
	EXPECT_TRUE(GetMapBugs("Binary", 2022597, 0x0ae3a3d5).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugs("Binarx", 2022597, 0x0ae3a3d5).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugs("Binary", 2022597, 0x0ae3a3d6).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugs("Binary", 2022598, 0x0ae3a3d5).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugs("dm1", 5805, 0xf2159e6e).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
}

TEST(MapBugs, Dump)
{
	GetMapBugs("Binary", 2022597, 0x0ae3a3d5).Dump();
	GetMapBugs("dm1", 5805, 0xf2159e6e).Dump();
}
