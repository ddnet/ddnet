#include <gtest/gtest.h>

#include <base/system.h>
#include <game/version.h>

TEST(GitRevision, ExistsOrNull)
{
	if(GIT_SHORTREV_HASH)
	{
		ASSERT_STRNE(GIT_SHORTREV_HASH, "");
	}
}
