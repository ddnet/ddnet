#include <gtest/gtest.h>

#include <engine/server/name_ban.h>

TEST(NameBan, Empty)
{
	CNameBans Bans;
	EXPECT_FALSE(Bans.IsBanned(""));
	EXPECT_FALSE(Bans.IsBanned("abc"));
}

TEST(NameBan, BanInfo)
{
	CNameBans Bans;

	Bans.Ban("abc", "old reason", 1, false);
	{
		const CNameBan *pOld = Bans.IsBanned("abc");
		ASSERT_TRUE(pOld);
		EXPECT_STREQ(pOld->m_aName, "abc");
		EXPECT_STREQ(pOld->m_aReason, "old reason");
		EXPECT_EQ(pOld->m_Distance, 1);
		EXPECT_EQ(pOld->m_IsSubstring, false);
	}

	// Update existing name ban
	Bans.Ban("abc", "new reason", 2, true);
	{
		const CNameBan *pNew = Bans.IsBanned("abc");
		ASSERT_TRUE(pNew);
		EXPECT_STREQ(pNew->m_aName, "abc");
		EXPECT_STREQ(pNew->m_aReason, "new reason");
		EXPECT_EQ(pNew->m_Distance, 2);
		EXPECT_EQ(pNew->m_IsSubstring, true);
	}
}

TEST(NameBan, Equality)
{
	CNameBans Bans;
	Bans.Ban("abc", "", 0, false);
	EXPECT_TRUE(Bans.IsBanned("abc"));
	EXPECT_TRUE(Bans.IsBanned("   abc"));
	EXPECT_TRUE(Bans.IsBanned("abc   "));
	EXPECT_TRUE(Bans.IsBanned("abc                   foo")); // Maximum name length.
	EXPECT_TRUE(Bans.IsBanned("äbc")); // Confusables
	EXPECT_FALSE(Bans.IsBanned("def"));
	EXPECT_FALSE(Bans.IsBanned("abcdef"));
}

TEST(NameBan, Substring)
{
	CNameBans Bans;
	Bans.Ban("xyz", "", 0, true);
	EXPECT_TRUE(Bans.IsBanned("abcxyz"));
	EXPECT_TRUE(Bans.IsBanned("abcxyzdef"));
	EXPECT_FALSE(Bans.IsBanned("abcdef"));
}

TEST(NameBan, Unban)
{
	CNameBans Bans;
	Bans.Ban("abc", "", 0, false);
	Bans.Ban("xyz", "", 0, false);
	EXPECT_TRUE(Bans.IsBanned("abc"));
	EXPECT_TRUE(Bans.IsBanned("xyz"));
	Bans.Unban("abc");
	EXPECT_FALSE(Bans.IsBanned("abc"));
	EXPECT_TRUE(Bans.IsBanned("xyz"));
	Bans.Unban("xyz");
	EXPECT_FALSE(Bans.IsBanned("abc"));
	EXPECT_FALSE(Bans.IsBanned("xyz"));
}

TEST(NameBan, UnbanNotFound)
{
	// Try to remove a name ban that does not exist
	CNameBans Bans;
	Bans.Unban("abc");
}
