#include <gtest/gtest.h>

#include <engine/server/name_ban.h>

TEST(NameBan, Empty)
{
	EXPECT_FALSE(IsNameBanned("", 0, 0));
	EXPECT_FALSE(IsNameBanned("abc", 0, 0));
}

TEST(NameBan, Equality)
{
	CNameBan Abc0("abc", 0, 0);
	EXPECT_TRUE(IsNameBanned("abc", &Abc0, 1));
	EXPECT_TRUE(IsNameBanned("   abc", &Abc0, 1));
	EXPECT_TRUE(IsNameBanned("abc   ", &Abc0, 1));
	EXPECT_TRUE(IsNameBanned("abc                   foo", &Abc0, 1)); // Maximum name length.
	EXPECT_TRUE(IsNameBanned("äbc", &Abc0, 1)); // Confusables
	EXPECT_FALSE(IsNameBanned("def", &Abc0, 1));
	EXPECT_FALSE(IsNameBanned("abcdef", &Abc0, 1));
}

TEST(NameBan, Substring)
{
	CNameBan Xyz("xyz", 0, 1);
	EXPECT_TRUE(IsNameBanned("abcxyz", &Xyz, 1));
	EXPECT_TRUE(IsNameBanned("abcxyzdef", &Xyz, 1));
	EXPECT_FALSE(IsNameBanned("abcdef", &Xyz, 1));
}
