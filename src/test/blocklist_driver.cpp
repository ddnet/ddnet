#include <gtest/gtest.h>

#include <engine/client/blocklist_driver.h>

TEST(BlocklistDriver, Valid1)
{
	int Major = -1, Minor = -1, Patch = -1;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7810", Major, Minor, Patch), "This Intel driver version can cause crashes, please update it to a newer version.");
	EXPECT_EQ(Major, 2);
	EXPECT_EQ(Minor, 0);
	EXPECT_EQ(Patch, 0);
}

TEST(BlocklistDriver, Valid2)
{
	int Major = -1, Minor = -1, Patch = -1;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7926", Major, Minor, Patch), "This Intel driver version can cause crashes, please update it to a newer version.");
	EXPECT_EQ(Major, 2);
	EXPECT_EQ(Minor, 0);
	EXPECT_EQ(Patch, 0);
}

TEST(BlocklistDriver, Valid3)
{
	int Major = -1, Minor = -1, Patch = -1;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7985", Major, Minor, Patch), "This Intel driver version can cause crashes, please update it to a newer version.");
	EXPECT_EQ(Major, 2);
	EXPECT_EQ(Minor, 0);
	EXPECT_EQ(Patch, 0);
}

TEST(BlocklistDriver, Invalid)
{
	int Major, Minor, Patch;
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 25.20.100.7810", Major, Minor, Patch), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7799", Major, Minor, Patch), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.8000", Major, Minor, Patch), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 27.20.100.7900", Major, Minor, Patch), NULL);
}
