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

TEST(MapBugs, Update)
{
	{
		CMapBugs Binary = GetMapBugs("Binary", 2022597, 0x0ae3a3d5);
		EXPECT_EQ(Binary.Update("grenade-doubleexplosion@ddnet.tw"), MAPBUGUPDATE_OVERRIDDEN);
		EXPECT_EQ(Binary.Update("doesntexist@invalid"), MAPBUGUPDATE_NOTFOUND);
		EXPECT_TRUE(Binary.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	}
	{
		CMapBugs Dm1 = GetMapBugs("dm1", 5805, 0xf2159e6e);
		EXPECT_FALSE(Dm1.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
		EXPECT_EQ(Dm1.Update("doesntexist@invalid"), MAPBUGUPDATE_NOTFOUND);
		EXPECT_FALSE(Dm1.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
		EXPECT_EQ(Dm1.Update("grenade-doubleexplosion@ddnet.tw"), MAPBUGUPDATE_OK);
		EXPECT_TRUE(Dm1.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	}
}

TEST(MapBugs, Dump)
{
	GetMapBugs("Binary", 2022597, 0x0ae3a3d5).Dump();
	GetMapBugs("dm1", 5805, 0xf2159e6e).Dump();
}
