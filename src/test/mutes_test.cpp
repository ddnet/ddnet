#include <base/types.h>

#include <game/server/gamecontext.h>

#include <gtest/gtest.h>

static NETADDR TestAddr()
{
	NETADDR Addr = {};
	Addr.type = NETTYPE_IPV4;
	Addr.ip[0] = 127;
	Addr.ip[3] = 1;
	Addr.port = 12345;
	return Addr;
}

TEST(Mutes, RegularMuteAppliesWhenIgnoringInitialDelay)
{
	CMutes Mutes("mutes_test");
	const NETADDR Addr = TestAddr();
	ASSERT_TRUE(Mutes.Mute(&Addr, 60, "Spam protection", "nameless tee", false));
	EXPECT_TRUE(Mutes.IsMuted(&Addr, true).has_value());
	EXPECT_TRUE(Mutes.IsMuted(&Addr, false).has_value());
}

TEST(Mutes, InitialDelayMuteIgnoredWhenIgnoringInitialDelay)
{
	CMutes Mutes("mutes_test");
	const NETADDR Addr = TestAddr();
	ASSERT_TRUE(Mutes.Mute(&Addr, 60, "Initial chat delay", "nameless tee", true));
	EXPECT_TRUE(Mutes.IsMuted(&Addr, true).has_value());
	EXPECT_FALSE(Mutes.IsMuted(&Addr, false).has_value());
}
