#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

TEST(Os, VersionStr)
{
	char aVersion[128];
	EXPECT_FALSE(os_version_str(aVersion, sizeof(aVersion)));
	EXPECT_STRNE(aVersion, "");
}
