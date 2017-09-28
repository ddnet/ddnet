#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/storage.h>

class StripPathAndExtension : public ::testing::Test
{
protected:
	void Test(const char *pInput, const char *pOutput)
	{
		char aBuf[32];
		IStorage::StripPathAndExtension(pInput, aBuf, sizeof(aBuf));
		EXPECT_STREQ(aBuf, pOutput);
	}
};

TEST_F(StripPathAndExtension, WorksOnBareFilename)
{
	Test("abc", "abc");
}

TEST_F(StripPathAndExtension, NormalPath)
{
	Test("/usr/share/teeworlds/data/mapres/grass_main.png", "grass_main");
}

TEST_F(StripPathAndExtension, NormalFile)
{
	Test("winter_main.png", "winter_main");
}

TEST_F(StripPathAndExtension, DotInFolder)
{
	Test("C:\\a.b\\c", "c");
}

TEST_F(StripPathAndExtension, DoubleDot)
{
	Test("file.name.png", "file.name");
}
