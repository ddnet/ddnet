#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

TEST(Os, VersionStr)
{
	char aVersion[128];
	ASSERT_TRUE(os_version_str(aVersion, sizeof(aVersion)));
	EXPECT_STRNE(aVersion, "");
}

TEST(Os, LocaleStr)
{
	char aLocale[128];
	os_locale_str(aLocale, sizeof(aLocale));
	EXPECT_STRNE(aLocale, "");
}

TEST(Os, HasElevatedPrivileges)
{
	// Only check that the function returns. This currently returns false in Unix runners but
	// true in Windows runners and it could also change in the future without warning.
	// https://docs.github.com/en/actions/using-github-hosted-runners/about-github-hosted-runners#administrative-privileges
	(void)os_has_elevated_privileges();
}
