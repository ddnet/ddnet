#include <base/net.h>

#include <gtest/gtest.h>

TEST(NetAddr, FromUrlStringInvalid)
{
	NETADDR Addr;

	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+udp://127.0", nullptr, 0), -1); // invalid ip
	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+udp://ddnet.org", nullptr, 0), -1); // invalid ip
	EXPECT_EQ(net_addr_from_url(&Addr, "127.0.0.1", nullptr, 0), 1); // not a URL
	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.9+udp://127.0.0.1", nullptr, 0), 1); // invalid tw protocol
	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+tcp://127.0.0.1", nullptr, 0), 1); // invalid internet protocol
	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+ws://127.0", nullptr, 0), -1); // invalid ip
	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+wss://", nullptr, 0), -1); // missing ip
	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-21+ws://127.0.0.1", nullptr, 0), 1); // invalid ddnet protocol
	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+udp://127.0.0.1", nullptr, 0), 1); // invalid internet protocol
	EXPECT_EQ(net_addr_from_url(&Addr, "wsss://127.0.0.1", nullptr, 0), 1); // invalid scheme
	EXPECT_EQ(net_addr_from_url(&Addr, "http://127.0.0.1", nullptr, 0), 1); // invalid scheme
	EXPECT_EQ(net_addr_from_url(&Addr, "ws://127.0.0.1:8303", nullptr, 0), 1); // only the ddnet-20+ schemes are supported
	EXPECT_EQ(net_addr_from_url(&Addr, "wss://127.0.0.1:8303", nullptr, 0), 1); // only the ddnet-20+ schemes are supported
}

TEST(NetAddr, FromUrlStringValid)
{
	NETADDR Addr;
	char aBuf1[NETADDR_MAXSTRSIZE];
	char aBuf2[NETADDR_MAXSTRSIZE];

	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.7+udp://127.0.0.1", nullptr, 0), 0);
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "127.0.0.1:0");
	EXPECT_STREQ(aBuf2, "127.0.0.1");

	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+udp://127.0.0.1", nullptr, 0), 0);
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "127.0.0.1:0");
	EXPECT_STREQ(aBuf2, "127.0.0.1");

	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+udp://127.0.0.1", nullptr, 0), 0);
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "127.0.0.1:0");
	EXPECT_STREQ(aBuf2, "127.0.0.1");

	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+udp://[0123:4567:89ab:cdef:1:2:3:4]:5678", nullptr, 0), 0);
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "[123:4567:89ab:cdef:1:2:3:4]:5678");
	EXPECT_STREQ(aBuf2, "[123:4567:89ab:cdef:1:2:3:4]");

	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+ws://127.0.0.1:8303", nullptr, 0), 0);
	EXPECT_EQ(Addr.type, NETTYPE_WEBSOCKET_IPV4);
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	EXPECT_STREQ(aBuf1, "127.0.0.1:8303");

	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+wss://[0123:4567:89ab:cdef:1:2:3:4]:5678", nullptr, 0), 0);
	EXPECT_EQ(Addr.type, NETTYPE_WEBSOCKET_IPV6 | NETTYPE_WEBSOCKET_TLS);
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	EXPECT_STREQ(aBuf1, "[123:4567:89ab:cdef:1:2:3:4]:5678");

	char aHost[128];
	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.6+udp://ger10.ddnet.org:5678", aHost, sizeof(aHost)), -1);
	EXPECT_STREQ(aHost, "ger10.ddnet.org:5678");

	// The types of the scheme are also reported when the host is not an IP address.
	// The address family of websocket addresses is only known after resolving the host.
	EXPECT_EQ(net_addr_from_url(&Addr, "tw-0.7+udp://ger10.ddnet.org:5678", aHost, sizeof(aHost)), -1);
	EXPECT_EQ(Addr.type, NETTYPE_TW7);
	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+ws://ger10.ddnet.org:5678", aHost, sizeof(aHost)), -1);
	EXPECT_EQ(Addr.type, NETTYPE_WEBSOCKET_IPV4 | NETTYPE_WEBSOCKET_IPV6);
	EXPECT_EQ(net_addr_from_url(&Addr, "ddnet-20+wss://ger10.ddnet.org:5678", aHost, sizeof(aHost)), -1);
	EXPECT_EQ(Addr.type, NETTYPE_WEBSOCKET_IPV4 | NETTYPE_WEBSOCKET_IPV6 | NETTYPE_WEBSOCKET_TLS);
}

TEST(NetAddr, FromUrlLookup)
{
	NETADDR Addr;

	EXPECT_EQ(net_addr_from_url_lookup(&Addr, "127.0.0.1:8303", NETTYPE_ALL), 0);
	EXPECT_EQ(Addr.type, NETTYPE_IPV4);
	EXPECT_EQ(Addr.port, 8303);

	EXPECT_EQ(net_addr_from_url_lookup(&Addr, "tw-0.7+udp://127.0.0.1:8303", NETTYPE_ALL), 0);
	EXPECT_EQ(Addr.type, NETTYPE_IPV4 | NETTYPE_TW7);

	EXPECT_EQ(net_addr_from_url_lookup(&Addr, "ddnet-20+ws://127.0.0.1:8303", NETTYPE_ALL), 0);
	EXPECT_EQ(Addr.type, NETTYPE_WEBSOCKET_IPV4);

	EXPECT_EQ(net_addr_from_url_lookup(&Addr, "ddnet-20+wss://[::1]:8303", NETTYPE_ALL), 0);
	EXPECT_EQ(Addr.type, NETTYPE_WEBSOCKET_IPV6 | NETTYPE_WEBSOCKET_TLS);
}

TEST(NetAddr, UrlStr)
{
	NETADDR Addr;
	char aBuf[NETADDR_URL_MAXSTRSIZE];

	// 0.6 addresses using UDP are formatted without a scheme, for backward compatibility.
	EXPECT_FALSE(net_addr_from_str(&Addr, "127.0.0.1:8303"));
	net_addr_url_str(&Addr, aBuf, sizeof(aBuf), true);
	EXPECT_STREQ(aBuf, "127.0.0.1:8303");
	net_addr_url_str(&Addr, aBuf, sizeof(aBuf), false);
	EXPECT_STREQ(aBuf, "127.0.0.1");

	static const char *const apUrls[] = {
		"tw-0.7+udp://127.0.0.1:8303",
		"ddnet-20+ws://127.0.0.1:8303",
		"ddnet-20+wss://127.0.0.1:8303",
		"ddnet-20+ws://[::1]:8303",
		"ddnet-20+wss://[123:4567:89ab:cdef:1:2:3:4]:5678",
	};
	for(const char *pUrl : apUrls)
	{
		EXPECT_EQ(net_addr_from_url(&Addr, pUrl, nullptr, 0), 0);
		net_addr_url_str(&Addr, aBuf, sizeof(aBuf), true);
		EXPECT_STREQ(aBuf, pUrl);
	}
}

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
	EXPECT_STREQ(aBuf1, "[::1]:0");
	EXPECT_STREQ(aBuf2, "[::1]");

	EXPECT_FALSE(net_addr_from_str(&Addr, "[0123:4567:89ab:cdef:1:2:3:4]:5678"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "[123:4567:89ab:cdef:1:2:3:4]:5678");
	EXPECT_STREQ(aBuf2, "[123:4567:89ab:cdef:1:2:3:4]");
}

TEST(NetAddr, StrV6)
{
	NETADDR Addr;
	char aBuf1[NETADDR_MAXSTRSIZE];
	char aBuf2[NETADDR_MAXSTRSIZE];

	// Test vectors from RFC 5952 section 4:
	// https://tools.ietf.org/html/rfc5952#section-4
	// 4.1 Handling Leading Zeros in a 16-Bit Field
	EXPECT_FALSE(net_addr_from_str(&Addr, "[2001:0db8::0001]:1"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "[2001:db8::1]:1");
	EXPECT_STREQ(aBuf2, "[2001:db8::1]");

	// 4.2.1 Shorten as Much as Possible
	EXPECT_FALSE(net_addr_from_str(&Addr, "[2001:db8:0:0:0:0:2:1]"));
	net_addr_str(&Addr, aBuf1, sizeof(aBuf1), true);
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf1, "[2001:db8::2:1]:0");
	EXPECT_STREQ(aBuf2, "[2001:db8::2:1]");

	EXPECT_FALSE(net_addr_from_str(&Addr, "[2001:db8::0:1]"));
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf2, "[2001:db8::1]");

	// 4.2.2 Handling One 16-Bit 0 Field
	EXPECT_FALSE(net_addr_from_str(&Addr, "[2001:db8::1:1:1:1:1]"));
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf2, "[2001:db8:0:1:1:1:1:1]");

	// 4.2.3 Choice in Placement of "::"
	EXPECT_FALSE(net_addr_from_str(&Addr, "[2001:0:0:1:0:0:0:1]"));
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf2, "[2001:0:0:1::1]");

	EXPECT_FALSE(net_addr_from_str(&Addr, "[2001:db8:0:0:1:0:0:1]"));
	net_addr_str(&Addr, aBuf2, sizeof(aBuf2), false);
	EXPECT_STREQ(aBuf2, "[2001:db8::1:0:0:1]");
}

TEST(NetAddr, FromStrInvalid)
{
	NETADDR Addr;
	EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0."));
	EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0.1a"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "1.1"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::1"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0.1:"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::]:"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "127.0.0.1:1a"));
	EXPECT_TRUE(net_addr_from_str(&Addr, "[::]:c"));
}

TEST(NetAddrDeathTest, StrInvalid1)
{
	ASSERT_DEATH({
		NETADDR Addr = {0};
		char aBuf[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aBuf, sizeof(aBuf), true); }, "");
}

TEST(NetAddrDeathTest, StrInvalid2)
{
	ASSERT_DEATH({
		NETADDR Addr = {0};
		char aBuf[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aBuf, sizeof(aBuf), false); }, "");
}

TEST(NetAddr, IsLocal)
{
	NETADDR Addr;
	net_addr_from_str(&Addr, "127.0.0.1");
	EXPECT_TRUE(net_addr_is_local(&Addr));

	net_addr_from_str(&Addr, "[::1]");
	EXPECT_TRUE(net_addr_is_local(&Addr));

	net_addr_from_str(&Addr, "192.168.1.1");
	EXPECT_TRUE(net_addr_is_local(&Addr));

	net_addr_from_str(&Addr, "10.0.0.1");
	EXPECT_TRUE(net_addr_is_local(&Addr));

	net_addr_from_str(&Addr, "8.8.8.8");
	EXPECT_FALSE(net_addr_is_local(&Addr));

	net_addr_from_str(&Addr, "[2001:db8::1]");
	EXPECT_FALSE(net_addr_is_local(&Addr));
}
