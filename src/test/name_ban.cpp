#include <gtest/gtest.h>

#include <engine/server/name_ban.h>

TEST(NameBan, Empty)
{
	std::vector<CNameBan> vBans;
	EXPECT_FALSE(IsNameBanned("", vBans));
	EXPECT_FALSE(IsNameBanned("abc", vBans));
}

TEST(NameBan, Equality)
{
	std::vector<CNameBan> vBans;
	vBans.emplace_back("abc", 0, 0);
	EXPECT_TRUE(IsNameBanned("abc", vBans));
	EXPECT_TRUE(IsNameBanned("   abc", vBans));
	EXPECT_TRUE(IsNameBanned("abc   ", vBans));
	EXPECT_TRUE(IsNameBanned("abc                   foo", vBans)); // Maximum name length.
	EXPECT_TRUE(IsNameBanned("äbc", vBans)); // Confusables
	EXPECT_FALSE(IsNameBanned("def", vBans));
	EXPECT_FALSE(IsNameBanned("abcdef", vBans));
}

TEST(NameBan, Substring)
{
	std::vector<CNameBan> vBans;
	vBans.emplace_back("xyz", 0, 1);
	EXPECT_TRUE(IsNameBanned("abcxyz", vBans));
	EXPECT_TRUE(IsNameBanned("abcxyzdef", vBans));
	EXPECT_FALSE(IsNameBanned("abcdef", vBans));
}
