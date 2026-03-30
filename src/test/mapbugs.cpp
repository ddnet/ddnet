#include <gtest/gtest.h>

#include <base/system.h>
#include <game/mapbugs.h>

static const char *BINARY_SHA256 = "65b410e197fd2298ec270e89a84b762f6739d1d18089529f8ef6cf2104d3d600";
static const char *DM1_SHA256 = "0b0c481d77519c32fbe85624ef16ec0fa9991aec7367ad538bd280f28d8c26cf";

static CMapBugs GetMapBugsImpl(const char *pName, int Size, const char *pSha256)
{
	SHA256_DIGEST Sha256 = {{0}};
	dbg_assert(sha256_from_str(&Sha256, pSha256) == 0, "invalid sha256 in tests");
	return GetMapBugs(pName, Size, Sha256);
}

TEST(MapBugs, Contains)
{
	EXPECT_TRUE(GetMapBugsImpl("Binary", 2022597, BINARY_SHA256).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugsImpl("Binarx", 2022597, BINARY_SHA256).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugsImpl("Binary", 2022598, BINARY_SHA256).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	EXPECT_FALSE(GetMapBugsImpl("dm1", 5805, DM1_SHA256).Contains(BUG_GRENADE_DOUBLEEXPLOSION));
}

TEST(MapBugs, Update)
{
	{
		CMapBugs Binary = GetMapBugsImpl("Binary", 2022597, BINARY_SHA256);
		EXPECT_EQ(Binary.Update("grenade-doubleexplosion@ddnet.tw"), MAPBUGUPDATE_OVERRIDDEN);
		EXPECT_EQ(Binary.Update("doesntexist@invalid"), MAPBUGUPDATE_NOTFOUND);
		EXPECT_TRUE(Binary.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	}
	{
		CMapBugs Dm1 = GetMapBugsImpl("dm1", 5805, DM1_SHA256);
		EXPECT_FALSE(Dm1.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
		EXPECT_EQ(Dm1.Update("doesntexist@invalid"), MAPBUGUPDATE_NOTFOUND);
		EXPECT_FALSE(Dm1.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
		EXPECT_EQ(Dm1.Update("grenade-doubleexplosion@ddnet.tw"), MAPBUGUPDATE_OK);
		EXPECT_TRUE(Dm1.Contains(BUG_GRENADE_DOUBLEEXPLOSION));
	}
}

TEST(MapBugs, Dump)
{
	GetMapBugsImpl("Binary", 2022597, BINARY_SHA256).Dump();
	GetMapBugsImpl("dm1", 5805, DM1_SHA256).Dump();
}
