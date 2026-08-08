#include "test.h"

#include <base/net.h>

#include <engine/client/serverbrowser.h>
#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <gtest/gtest.h>

#include <memory>

TEST(ServerBrowser, CanConnectAddress)
{
	NETADDR Udp6, Udp7, Ws4, Ws6, Wss4;
	ASSERT_FALSE(net_addr_from_url(&Udp6, "tw-0.6+udp://127.0.0.1:8303", nullptr, 0));
	ASSERT_FALSE(net_addr_from_url(&Udp7, "tw-0.7+udp://127.0.0.1:8303", nullptr, 0));
	ASSERT_FALSE(net_addr_from_url(&Ws4, "ddnet-20+ws://127.0.0.1:8303", nullptr, 0));
	ASSERT_FALSE(net_addr_from_url(&Ws6, "ddnet-20+ws://[::1]:8303", nullptr, 0));
	ASSERT_FALSE(net_addr_from_url(&Wss4, "ddnet-20+wss://127.0.0.1:8303", nullptr, 0));

	static const int UDP_ONLY = NETTYPE_IPV4 | NETTYPE_IPV6;
	static const int WITH_WEBSOCKETS = UDP_ONLY | NETTYPE_WEBSOCKET_IPV4 | NETTYPE_WEBSOCKET_IPV6;

	// Native client without websocket support.
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Udp6, UDP_ONLY, false, false));
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Udp7, UDP_ONLY, false, false));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Ws4, UDP_ONLY, false, false));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Wss4, UDP_ONLY, false, false));

	// Native client with websocket support; the address family must match.
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Ws4, WITH_WEBSOCKETS, false, false));
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Ws6, WITH_WEBSOCKETS, false, false));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Ws6, UDP_ONLY | NETTYPE_WEBSOCKET_IPV4, false, false));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Ws4, UDP_ONLY | NETTYPE_WEBSOCKET_IPV6, false, false));
	// Secure websockets are not supported by the native client.
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Wss4, WITH_WEBSOCKETS, false, false));

	// Browser client on an http page.
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Ws4, 0, true, false));
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Ws6, 0, true, false));
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Wss4, 0, true, false));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Udp6, 0, true, false));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Udp7, 0, true, false));

	// Browser client on an https page blocks insecure websockets as mixed content.
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Ws4, 0, true, true));
	EXPECT_TRUE(ServerBrowserCanConnectAddress(Wss4, 0, true, true));
	EXPECT_FALSE(ServerBrowserCanConnectAddress(Udp6, 0, true, true));
}

TEST(ServerBrowser, PingCache)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;

	auto pConsole = CreateConsole(CFGFLAG_CLIENT);
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr) << "Error creating test storage";
	auto pPingCache = std::unique_ptr<IServerBrowserPingCache>(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	NETADDR Localhost4, Localhost6, OtherLocalhost4, OtherLocalhost6;
	ASSERT_FALSE(net_addr_from_str(&Localhost4, "127.0.0.1:8303"));
	ASSERT_FALSE(net_addr_from_str(&Localhost6, "[::1]:8304"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost4, "127.0.0.1:8305"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost6, "[::1]:8306"));
	EXPECT_LT(net_addr_comp(&Localhost4, &Localhost6), 0);
	NETADDR aLocalhostBoth[2] = {Localhost4, Localhost6};

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->Load();

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	// Newer pings overwrite older.
	pPingCache->CachePing(Localhost4, 123);
	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost4, 345);
	pPingCache->CachePing(Localhost4, 456);
	pPingCache->CachePing(Localhost4, 567);
	pPingCache->CachePing(Localhost4, 678);
	pPingCache->CachePing(Localhost4, 789);
	pPingCache->CachePing(Localhost4, 890);
	pPingCache->CachePing(Localhost4, 901);
	pPingCache->CachePing(Localhost4, 135);
	pPingCache->CachePing(Localhost4, 246);
	pPingCache->CachePing(Localhost4, 357);
	pPingCache->CachePing(Localhost4, 468);
	pPingCache->CachePing(Localhost4, 579);
	pPingCache->CachePing(Localhost4, 680);
	pPingCache->CachePing(Localhost4, 791);
	pPingCache->CachePing(Localhost4, 802);
	pPingCache->CachePing(Localhost4, 913);

	EXPECT_EQ(pPingCache->NumEntries(), 1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost6, 345);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	// Port doesn't matter for overwriting.
	pPingCache->CachePing(Localhost4, 1337);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	pPingCache.reset(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	// Persistence.
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);
}
