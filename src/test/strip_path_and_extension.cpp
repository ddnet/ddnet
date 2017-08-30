#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/storage.h>

#define T(name, input, output) \
	TEST(StripPathAndExtension, name) \
	{ \
		char aBuf[32]; \
		IStorage::StripPathAndExtension(input, aBuf, sizeof(aBuf)); \
		ASSERT_STREQ(aBuf, output); \
	}

T(WorksOnBareFilename, "abc", "abc");
T(NormalPath, "/usr/share/teeworlds/data/mapres/grass_main.png", "grass_main");
T(NormalFile, "winter_main.png", "winter_main");
T(DotInFolder, "C:\\a.b\\c", "c");
T(DoubleDot, "file.name.png", "file.name");
