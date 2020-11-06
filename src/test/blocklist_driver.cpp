#include <gtest/gtest.h>

#include <engine/client/blocklist_driver.h>

TEST(BlocklistDriver, Valid)
{
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7810"), "This Intel driver version can cause crashes, please update it to a newer version and remove any gfx_opengl* config from ddnet_settings.cfg.");
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7926"), "This Intel driver version can cause crashes, please update it to a newer version and remove any gfx_opengl* config from ddnet_settings.cfg.");
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7985"), "This Intel driver version can cause crashes, please update it to a newer version and remove any gfx_opengl* config from ddnet_settings.cfg.");
}

TEST(BlocklistDriver, Invalid)
{
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 25.20.100.7810"), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.7799"), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 26.20.100.8000"), NULL);
	EXPECT_STREQ(ParseBlocklistDriverVersions("Intel", "Build 27.20.100.7900"), NULL);
}
