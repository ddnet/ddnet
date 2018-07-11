#include <gtest/gtest.h>

#include <engine/serverbrowser.h>
#include <engine/shared/serverinfo.h>

#include <engine/external/json-parser/json.h>

TEST(ServerInfo, ParseLocation)
{
	int Result;
	EXPECT_TRUE(CServerInfo::ParseLocation(&Result, "xx"));
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "an"));
	EXPECT_EQ(Result, CServerInfo::LOC_UNKNOWN);
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "af"));
	EXPECT_EQ(Result, CServerInfo::LOC_AFRICA);
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "eu-n"));
	EXPECT_EQ(Result, CServerInfo::LOC_EUROPE);
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "na"));
	EXPECT_EQ(Result, CServerInfo::LOC_NORTH_AMERICA);
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "sa"));
	EXPECT_EQ(Result, CServerInfo::LOC_SOUTH_AMERICA);
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "as:e"));
	EXPECT_EQ(Result, CServerInfo::LOC_ASIA);
	EXPECT_FALSE(CServerInfo::ParseLocation(&Result, "oc"));
	EXPECT_EQ(Result, CServerInfo::LOC_AUSTRALIA);
}

/*
static CServerInfo2 Parse(const char *pJson)
{
	CServerInfo2 Out = {0};
	json_value *pParsed = json_parse(pJson, str_length(pJson));
	EXPECT_TRUE(pParsed);
	if(pParsed)
	{
		EXPECT_FALSE(CServerInfo2::FromJson(&Out, pParsed));
	}
	return Out;
}

TEST(ServerInfo, Empty)
{
	static const char EMPTY[] = "{\"max_clients\":0,\"max_players\":0,\"passworded\":false,\"game_type\":\"\",\"name\":\"\",\"map\":{\"name\":\"\",\"crc\":\"00000000\",\"size\":0},\"version\":\"\",\"clients\":[]}";

	CServerInfo2 Empty = {0};
	EXPECT_EQ(Parse(EMPTY), Empty);
	char aBuf[1024];
	Empty.ToJson(aBuf, sizeof(aBuf));
	EXPECT_STREQ(aBuf, EMPTY);
}

CServerInfo2 SomethingImpl()
{
	CServerInfo2 Something = {0};

	str_copy(Something.m_aClients[0].m_aName, "Learath2", sizeof(Something.m_aClients[0].m_aName));
	Something.m_aClients[0].m_Country = -1;
	Something.m_aClients[0].m_Score = -1;
	Something.m_aClients[0].m_Team = 1;

	str_copy(Something.m_aClients[1].m_aName, "deen", sizeof(Something.m_aClients[1].m_aName));
	str_copy(Something.m_aClients[1].m_aClan, "DDNet", sizeof(Something.m_aClients[1].m_aClan));
	Something.m_aClients[1].m_Country = 276;
	Something.m_aClients[1].m_Score = 0;
	Something.m_aClients[1].m_Team = 0;

	Something.m_MaxClients = 16;
	Something.m_NumClients = 2;
	Something.m_MaxPlayers = 8;
	Something.m_NumPlayers = 1;
	Something.m_Passworded = true;
	str_copy(Something.m_aGameType, "DM", sizeof(Something.m_aGameType));
	str_copy(Something.m_aName, "unnamed server", sizeof(Something.m_aName));
	str_copy(Something.m_aMapName, "dm1", sizeof(Something.m_aMapName));
	Something.m_MapCrc = 0xf2159e6e;
	Something.m_MapSize = 5805;
	str_copy(Something.m_aVersion, "0.6.4", sizeof(Something.m_aVersion));

	return Something;
}

static const CServerInfo2 s_Something = SomethingImpl();

TEST(ServerInfo, Something)
{
	static const char SOMETHING[] = "{\"max_clients\":16,\"max_players\":8,\"passworded\":true,\"game_type\":\"DM\",\"name\":\"unnamed server\",\"map\":{\"name\":\"dm1\",\"crc\":\"f2159e6e\",\"size\":5805},\"version\":\"0.6.4\",\"clients\":[{\"name\":\"Learath2\",\"clan\":\"\",\"country\":-1,\"score\":-1,\"team\":1},{\"name\":\"deen\",\"clan\":\"DDNet\",\"country\":276,\"score\":0,\"team\":0}]}";

	EXPECT_EQ(Parse(SOMETHING), s_Something);

	char aBuf[1024];
	s_Something.ToJson(aBuf, sizeof(aBuf));
	EXPECT_EQ(Parse(aBuf), s_Something);
}

TEST(ServerInfo, ToServerBrowserServerInfo)
{
	CServerInfo Sbsi = s_Something;

	EXPECT_EQ(Sbsi.m_MaxClients, s_Something.m_MaxClients);
	EXPECT_EQ(Sbsi.m_NumClients, s_Something.m_NumClients);
	EXPECT_EQ(Sbsi.m_MaxPlayers, s_Something.m_MaxPlayers);
	EXPECT_EQ(Sbsi.m_NumPlayers, s_Something.m_NumPlayers);
	EXPECT_EQ((Sbsi.m_Flags&SERVER_FLAG_PASSWORD) != 0, s_Something.m_Passworded);
	EXPECT_STREQ(Sbsi.m_aGameType, s_Something.m_aGameType);
	EXPECT_STREQ(Sbsi.m_aName, s_Something.m_aName);
	EXPECT_STREQ(Sbsi.m_aMap, s_Something.m_aMapName);
	EXPECT_EQ(Sbsi.m_MapCrc, s_Something.m_MapCrc);
	EXPECT_EQ(Sbsi.m_MapSize, s_Something.m_MapSize);
	EXPECT_STREQ(Sbsi.m_aVersion, s_Something.m_aVersion);

	for(int i = 0; i < Sbsi.m_NumClients; i++)
	{
		EXPECT_STREQ(Sbsi.m_aClients[i].m_aName, s_Something.m_aClients[i].m_aName);
		EXPECT_STREQ(Sbsi.m_aClients[i].m_aClan, s_Something.m_aClients[i].m_aClan);
		EXPECT_EQ(Sbsi.m_aClients[i].m_Country, s_Something.m_aClients[i].m_Country);
		EXPECT_EQ(Sbsi.m_aClients[i].m_Score, s_Something.m_aClients[i].m_Score);
		EXPECT_EQ(Sbsi.m_aClients[i].m_Player, s_Something.m_aClients[i].m_Team != 0);
	}
}
*/

static unsigned int ParseCrcOrDeadbeef(const char *pString)
{
	unsigned int Result;
	if(ParseCrc(&Result, pString))
	{
		Result = 0xdeadbeef;
	}
	return Result;
}

TEST(ServerInfo, Crc)
{
	EXPECT_EQ(ParseCrcOrDeadbeef("00000000"), 0);
	EXPECT_EQ(ParseCrcOrDeadbeef("00000001"), 1);
	EXPECT_EQ(ParseCrcOrDeadbeef("12345678"), 0x12345678);
	EXPECT_EQ(ParseCrcOrDeadbeef("9abcdef0"), 0x9abcdef0);

	EXPECT_EQ(ParseCrcOrDeadbeef(""), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("a"), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("x"), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("ç"), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("😢"), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("0"), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("000000000"), 0xdeadbeef);
	EXPECT_EQ(ParseCrcOrDeadbeef("00000000x"), 0xdeadbeef);
}
