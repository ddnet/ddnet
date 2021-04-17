#include <gtest/gtest.h>

#include <base/system.h>

TEST(NetAddr, FromStr)
{
	NETADDR Addr;
	char aBuf1[NETADDR_MAXSTRSIZE];
	char aBuf2[NETADDR_MAXSTRSIZE];

	EXPECT_FALSE(net_addr_from_str(&Addr, "127.0.0.1"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "127.0.0.1:0");
	EXPECT_STREQ(aBuf2, "127.0.0.1");

	EXPECT_FALSE(net_addr_from_str(&Addr, "1.2.3.4:5678"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "1.2.3.4:5678");
	EXPECT_STREQ(aBuf2, "1.2.3.4");

	EXPECT_FALSE(net_addr_from_str(&Addr, "[::1]"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "[0:0:0:0:0:0:0:1]:0");
	EXPECT_STREQ(aBuf2, "[0:0:0:0:0:0:0:1]");

	EXPECT_FALSE(net_addr_from_str(&Addr, "[0123:4567:89ab:cdef:1:2:3:4]:5678"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "[123:4567:89ab:cdef:1:2:3:4]:5678");
	EXPECT_STREQ(aBuf2, "[123:4567:89ab:cdef:1:2:3:4]");
}

TEST(NetAddr, FromStrInvalid)
{
	NETADDR Addr;
	EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0."));
	//EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0.1a"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "1.1"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::1"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0.1:"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::]:"));
	//EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0.1:1a"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::]:c"));
}
