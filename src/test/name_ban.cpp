#include <gtest/gtest.h>

#include <engine/server/name_ban.h>

TEST(NameBan, Empty)
{
	ASSERT_FALSE(IsNameBanned("", 0, 0));
	ASSERT_FALSE(IsNameBanned("abc", 0, 0));
}

TEST(NameBan, Equality)
{
	CNameBan Abc0("abc", 0);
	ASSERT_TRUE(IsNameBanned("abc", &Abc0, 1));
	ASSERT_TRUE(IsNameBanned("   abc", &Abc0, 1));
	ASSERT_TRUE(IsNameBanned("abc   ", &Abc0, 1));
	ASSERT_TRUE(IsNameBanned("abc                   foo", &Abc0, 1)); // Maximum name length.
	ASSERT_FALSE(IsNameBanned("def", &Abc0, 1));
	ASSERT_FALSE(IsNameBanned("abcdef", &Abc0, 1));
}
