#include <base/system.h>

#include <game/version.h>

#include <gtest/gtest.h>

TEST(GitRevision, ExistsOrNull)
{
	if(GIT_SHORTREV_HASH)
	{
		EXPECT_STRNE(GIT_SHORTREV_HASH, "");
	}
}
