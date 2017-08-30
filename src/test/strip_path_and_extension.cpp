#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/storage.h>

class StripPathAndExtension : public ::testing::Test
{
protected:
	void testEq(const char *pInput, const char *pOutput)
	{
		char aBuf[32];
		IStorage::StripPathAndExtension(pInput, aBuf, sizeof(aBuf));
		ASSERT_STREQ(aBuf, pOutput);
	}
};

TEST_F(StripPathAndExtension, WorksOnBareFilename) {
	testEq("abc", "abc");
}

TEST_F(StripPathAndExtension, NormalPath) {
	testEq("/usr/share/teeworlds/data/mapres/grass_main.png", "grass_main");
}

TEST_F(StripPathAndExtension, NormalFile) {
	testEq("winter_main.png", "winter_main");
}

TEST_F(StripPathAndExtension, DotInFolder) {
	testEq("C:\\a.b\\c", "c");
}

TEST_F(StripPathAndExtension, DoubleDot) {
	testEq("file.name.png", "file.name");
}
