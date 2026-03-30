#include <gtest/gtest.h>

#include <base/detect.h>

#include <engine/client/blocklist_driver.h>

TEST(BlocklistDriver, Valid1)
{
#ifdef CONF_FAMILY_WINDOWS
	int Major = -1, Minor = -1, Patch = -1;
	bool WarningReq = false;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7810", Major, Minor, Patch, WarningReq), "This Intel driver version can cause crashes, please update it to a newer version.");
	EXPECT_EQ(Major, 2);
	EXPECT_EQ(Minor, 0);
	EXPECT_EQ(Patch, 0);
#endif
}

TEST(BlocklistDriver, Valid2)
{
#ifdef CONF_FAMILY_WINDOWS
	int Major = -1, Minor = -1, Patch = -1;
	bool WarningReq = false;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7926", Major, Minor, Patch, WarningReq), "This Intel driver version can cause crashes, please update it to a newer version.");
	EXPECT_EQ(Major, 2);
	EXPECT_EQ(Minor, 0);
	EXPECT_EQ(Patch, 0);
#endif
}

TEST(BlocklistDriver, Valid3)
{
#ifdef CONF_FAMILY_WINDOWS
	int Major = -1, Minor = -1, Patch = -1;
	bool WarningReq = false;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7985", Major, Minor, Patch, WarningReq), "This Intel driver version can cause crashes, please update it to a newer version.");
	EXPECT_EQ(Major, 2);
	EXPECT_EQ(Minor, 0);
	EXPECT_EQ(Patch, 0);
#endif
}

TEST(BlocklistDriver, Invalid)
{
	int Major, Minor, Patch;
	bool WarningReq = false;
#ifdef CONF_FAMILY_WINDOWS
	EXPECT_STREQ(ParseBlocklistDriverVersions("AMD", "Build 25.20.100.7810", Major, Minor, Patch, WarningReq), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("NVIDIA", "Build 26.20.100.7799", Major, Minor, Patch, WarningReq), NULL);
#else
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7985", Major, Minor, Patch, WarningReq), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7799", Major, Minor, Patch, WarningReq), NULL);
#endif
}
