#include <gtest/gtest.h>

#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <test/test.h>

TEST(ServerBrowser, PingCache)
{
	CTestInfo Info;
	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT);
	IStorage *pStorage = Info.CreateTestStorage();
	IServerBrowserPingCache *pPingCache = CreateServerBrowserPingCache(pConsole, pStorage);

	const IServerBrowserPingCache::CEntry *pEntries;
	int NumEntries;

	pPingCache->GetPingCache(&pEntries, &NumEntries);
	EXPECT_EQ(NumEntries, 0);

	pPingCache->Load();

	pPingCache->GetPingCache(&pEntries, &NumEntries);
	EXPECT_EQ(NumEntries, 0);

	NETADDR Localhost4, Localhost4Tw, Localhost6, Localhost6Tw;
	ASSERT_FALSE(net_addr_from_str(&Localhost4, "127.0.0.1"));
	ASSERT_FALSE(net_addr_from_str(&Localhost4Tw, "127.0.0.1:8303"));
	ASSERT_FALSE(net_addr_from_str(&Localhost6, "[::1]"));
	ASSERT_FALSE(net_addr_from_str(&Localhost6Tw, "[::1]:8304"));
	EXPECT_LT(net_addr_comp(&Localhost4, &Localhost6), 0);

	// Newer pings overwrite older.
	pPingCache->CachePing(Localhost4Tw, 123);
	pPingCache->CachePing(Localhost4Tw, 234);
	pPingCache->CachePing(Localhost4Tw, 345);
	pPingCache->CachePing(Localhost4Tw, 456);
	pPingCache->CachePing(Localhost4Tw, 567);
	pPingCache->CachePing(Localhost4Tw, 678);
	pPingCache->CachePing(Localhost4Tw, 789);
	pPingCache->CachePing(Localhost4Tw, 890);
	pPingCache->CachePing(Localhost4Tw, 901);
	pPingCache->CachePing(Localhost4Tw, 135);
	pPingCache->CachePing(Localhost4Tw, 246);
	pPingCache->CachePing(Localhost4Tw, 357);
	pPingCache->CachePing(Localhost4Tw, 468);
	pPingCache->CachePing(Localhost4Tw, 579);
	pPingCache->CachePing(Localhost4Tw, 680);
	pPingCache->CachePing(Localhost4Tw, 791);
	pPingCache->CachePing(Localhost4Tw, 802);
	pPingCache->CachePing(Localhost4Tw, 913);

	pPingCache->GetPingCache(&pEntries, &NumEntries);
	EXPECT_EQ(NumEntries, 1);
	if(NumEntries >= 1)
	{
		EXPECT_TRUE(net_addr_comp(&pEntries[0].m_Addr, &Localhost4) == 0);
		EXPECT_EQ(pEntries[0].m_Ping, 913);
	}

	pPingCache->CachePing(Localhost4Tw, 234);
	pPingCache->CachePing(Localhost6Tw, 345);
	pPingCache->GetPingCache(&pEntries, &NumEntries);
	EXPECT_EQ(NumEntries, 2);
	if(NumEntries >= 2)
	{
		EXPECT_TRUE(net_addr_comp(&pEntries[0].m_Addr, &Localhost4) == 0);
		EXPECT_TRUE(net_addr_comp(&pEntries[1].m_Addr, &Localhost6) == 0);
		EXPECT_EQ(pEntries[0].m_Ping, 234);
		EXPECT_EQ(pEntries[1].m_Ping, 345);
	}

	// Port doesn't matter for overwriting.
	pPingCache->CachePing(Localhost4, 1337);
	pPingCache->GetPingCache(&pEntries, &NumEntries);
	EXPECT_EQ(NumEntries, 2);
	if(NumEntries >= 2)
	{
		EXPECT_TRUE(net_addr_comp(&pEntries[0].m_Addr, &Localhost4) == 0);
		EXPECT_TRUE(net_addr_comp(&pEntries[1].m_Addr, &Localhost6) == 0);
		EXPECT_EQ(pEntries[0].m_Ping, 1337);
		EXPECT_EQ(pEntries[1].m_Ping, 345);
	}

	delete pPingCache;
	pPingCache = CreateServerBrowserPingCache(pConsole, pStorage);

	// Persistence.
	pPingCache->Load();
	pPingCache->GetPingCache(&pEntries, &NumEntries);
	EXPECT_EQ(NumEntries, 2);
	if(NumEntries >= 2)
	{
		EXPECT_TRUE(net_addr_comp(&pEntries[0].m_Addr, &Localhost4) == 0);
		EXPECT_TRUE(net_addr_comp(&pEntries[1].m_Addr, &Localhost6) == 0);
		EXPECT_EQ(pEntries[0].m_Ping, 1337);
		EXPECT_EQ(pEntries[1].m_Ping, 345);
	}

	delete pPingCache;
	delete pStorage;

	Info.DeleteTestStorageFilesOnSuccess();
}
