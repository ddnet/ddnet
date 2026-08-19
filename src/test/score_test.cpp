#include <base/detect.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/shared/config.h>

#include <game/server/scoreworker.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>

#if defined(CONF_TEST_MYSQL)
int DummyMysqlInit = (MysqlInit(), 1);
#endif

TEST(SQLite, Version)
{
	ASSERT_GE(sqlite3_libversion_number(), 3025000) << "SQLite >= 3.25.0 required for Window functions";
}

struct Score : public testing::TestWithParam<IDbConnection *> // NOLINT(readability-identifier-naming)
{
	Score()
	{
		Connect();
		LoadBestTime();
		InsertMap("Kobra 3", "Zerodin", "Novice", 5, 5);
	}

	~Score() override
	{
		m_pConn->Disconnect();
	}

	void Connect()
	{
		ASSERT_TRUE(m_pConn->Connect(m_aError, sizeof(m_aError))) << m_aError;

		// Delete all existing entries for persistent databases like MySQL
		int NumInserted = 0;
		ASSERT_TRUE(m_pConn->PrepareStatement("DELETE FROM record_race", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->PrepareStatement("DELETE FROM record_teamrace", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->PrepareStatement("DELETE FROM record_maps", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->PrepareStatement("DELETE FROM record_points", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->PrepareStatement("DELETE FROM record_saves", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_TRUE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
	}

	void LoadBestTime()
	{
		CSqlLoadBestTimeRequest LoadBestTimeReq(std::make_shared<CScoreLoadBestTimeResult>());
		str_copy(LoadBestTimeReq.m_aMap, "Kobra 3");
		ASSERT_TRUE(CScoreWorker::LoadBestTime(m_pConn, &LoadBestTimeReq, m_aError, sizeof(m_aError))) << m_aError;
	}

	void InsertMap(const char *pName, const char *pMapper, const char *pServer, int Points, int Stars)
	{
		char aTimestamp[32];
		str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"%s into %s_maps(Map, Server, Mapper, Points, Stars, Timestamp) "
			"VALUES (\"%s\", \"%s\", \"%s\", %d, %d, %s)",
			m_pConn->InsertIgnore(), m_pConn->GetPrefix(), pName, pServer, pMapper, Points, Stars, m_pConn->InsertTimestampAsUtc());
		ASSERT_TRUE(m_pConn->PrepareStatement(aBuf, m_aError, sizeof(m_aError))) << m_aError;
		m_pConn->BindString(1, aTimestamp);
		int NumInserted = 0;
		ASSERT_TRUE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_EQ(NumInserted, 1);
	}

	void InsertRank(float Time = 100.0, bool WithTimeCheckPoints = false, const char *pName = "nameless tee")
	{
		str_copy(g_Config.m_SvSqlServerName, "USA");
		CSqlScoreData ScoreData(std::make_shared<CScorePlayerResult>());
		str_copy(ScoreData.m_aMap, "Kobra 3");
		str_copy(ScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320");
		str_copy(ScoreData.m_aName, pName);
		ScoreData.m_ClientId = 0;
		ScoreData.m_Time = Time;
		str_copy(ScoreData.m_aTimestamp, "2021-11-24 19:24:08");
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			ScoreData.m_aCurrentTimeCp[i] = WithTimeCheckPoints ? i : 0;
		str_copy(ScoreData.m_aRequestingPlayer, "deen");
		ASSERT_TRUE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
	}

	void ExpectLines(const std::shared_ptr<CScorePlayerResult> &pPlayerResult, std::initializer_list<const char *> Lines, bool All = false)
	{
		EXPECT_EQ(pPlayerResult->m_MessageKind, All ? CScorePlayerResult::ALL : CScorePlayerResult::DIRECT);

		int i = 0;
		for(const char *pLine : Lines)
		{
			EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[i], pLine);
			i++;
		}

		for(; i < CScorePlayerResult::MAX_MESSAGES; i++)
		{
			EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[i], "");
		}
	}

	IDbConnection *m_pConn{GetParam()};
	char m_aError[256] = {};
	std::shared_ptr<CScorePlayerResult> m_pPlayerResult{std::make_shared<CScorePlayerResult>()};
	CSqlPlayerRequest m_PlayerRequest{m_pPlayerResult};
};

struct SingleScore : public Score // NOLINT(readability-identifier-naming)
{
	SingleScore()
	{
		InsertRank();
		str_copy(m_PlayerRequest.m_aMap, "Kobra 3");
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee");
		m_PlayerRequest.m_Offset = 0;
		str_copy(m_PlayerRequest.m_aServer, "GER");
		str_copy(m_PlayerRequest.m_aName, "nameless tee", sizeof(m_PlayerRequest.m_aMap));
	}
};

TEST_P(SingleScore, TopRegional)
{
	g_Config.m_SvRegionalRankings = true;
	ASSERT_TRUE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"------------ GER Top ------------"});
}

TEST_P(SingleScore, Top)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_TRUE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"-----------------------------------------"});
}

TEST_P(SingleScore, RankRegional)
{
	g_Config.m_SvRegionalRankings = true;
	ASSERT_TRUE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1 - GER unranked"}, true);
}

TEST_P(SingleScore, Rank)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_TRUE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1"}, true);
}

TEST_P(SingleScore, TopServerRegional)
{
	g_Config.m_SvRegionalRankings = true;
	str_copy(m_PlayerRequest.m_aServer, "USA");
	ASSERT_TRUE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"------------ USA Top ------------",
			"1. nameless tee Time: 01:40.00"});
}

TEST_P(SingleScore, TopServer)
{
	g_Config.m_SvRegionalRankings = false;
	str_copy(m_PlayerRequest.m_aServer, "USA");
	ASSERT_TRUE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"-----------------------------------------"});
}

TEST_P(SingleScore, RankServerRegional)
{
	g_Config.m_SvRegionalRankings = true;
	str_copy(m_PlayerRequest.m_aServer, "USA");
	ASSERT_TRUE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1 - USA rank 1"}, true);
}

TEST_P(SingleScore, RankServer)
{
	g_Config.m_SvRegionalRankings = false;
	str_copy(m_PlayerRequest.m_aServer, "USA");
	ASSERT_TRUE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1"}, true);
}

TEST_P(SingleScore, RankPercent)
{
	g_Config.m_SvRegionalRankings = false;
	InsertRank(200.0, false, "second tee");
	InsertRank(300.0, false, "third tee");
	InsertRank(400.0, false, "fourth tee");
	str_copy(m_PlayerRequest.m_aName, "third tee");
	ASSERT_TRUE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"third tee - 05:00.00 - better than 33% - requested by brainless tee", "Global rank 3"}, true);
}

TEST_P(SingleScore, LoadPlayerData)
{
	InsertRank(120.0, true);
	str_copy(m_PlayerRequest.m_aName, "", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	ASSERT_TRUE(CScoreWorker::LoadPlayerData(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::PLAYER_INFO);
	ASSERT_FALSE(m_pPlayerResult->m_Data.m_Info.m_Time.has_value());
	for(auto &Time : m_pPlayerResult->m_Data.m_Info.m_aTimeCp)
	{
		ASSERT_EQ(Time, 0);
	}

	str_copy(m_PlayerRequest.m_aRequestingPlayer, "nameless tee");
	str_copy(m_PlayerRequest.m_aName, "", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	ASSERT_TRUE(CScoreWorker::LoadPlayerData(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::PLAYER_INFO);
	ASSERT_TRUE(m_pPlayerResult->m_Data.m_Info.m_Time.has_value());
	ASSERT_EQ(*m_pPlayerResult->m_Data.m_Info.m_Time, 100.0);
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
	{
		ASSERT_EQ(m_pPlayerResult->m_Data.m_Info.m_aTimeCp[i], i);
	}

	str_copy(m_PlayerRequest.m_aRequestingPlayer, "finishless");
	str_copy(m_PlayerRequest.m_aName, "nameless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	ASSERT_TRUE(CScoreWorker::LoadPlayerData(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::PLAYER_INFO);
	ASSERT_FALSE(m_pPlayerResult->m_Data.m_Info.m_Time.has_value());
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
	{
		ASSERT_EQ(m_pPlayerResult->m_Data.m_Info.m_aTimeCp[i], i);
	}
}

TEST_P(SingleScore, TimesExists)
{
	ASSERT_TRUE(CScoreWorker::ShowTimes(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[0], "------------- Last Times -------------");
	char aBuf[128];
	str_copy(aBuf, m_pPlayerResult->m_Data.m_aaMessages[1], 7);
	EXPECT_STREQ(aBuf, "[USA] ");

	str_copy(aBuf, m_pPlayerResult->m_Data.m_aaMessages[1] + str_length(m_pPlayerResult->m_Data.m_aaMessages[1]) - 10, 11);
	EXPECT_STREQ(aBuf, ", 01:40.00");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[2], "-------------------------------------------");
	for(int i = 3; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(SingleScore, TimesDoesntExist)
{
	str_copy(m_PlayerRequest.m_aName, "foo", sizeof(m_PlayerRequest.m_aMap));
	ASSERT_TRUE(CScoreWorker::ShowTimes(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"There are no times in the specified range"});
}

struct TeamScore : public Score // NOLINT(readability-identifier-naming)
{
	void SetUp() override
	{
		InsertTeamRank(100.0);
	}

	void InsertTeamRank(float Time = 100.0)
	{
		str_copy(g_Config.m_SvSqlServerName, "USA");
		CSqlTeamScoreData TeamScoreData;
		CSqlScoreData ScoreData(std::make_shared<CScorePlayerResult>());
		str_copy(TeamScoreData.m_aMap, "Kobra 3");
		str_copy(ScoreData.m_aMap, "Kobra 3");
		str_copy(TeamScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320");
		str_copy(ScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320");
		TeamScoreData.m_Size = 2;
		str_copy(TeamScoreData.m_aaNames[0], "nameless tee");
		str_copy(TeamScoreData.m_aaNames[1], "brainless tee");
		TeamScoreData.m_Time = Time;
		ScoreData.m_Time = Time;
		str_copy(TeamScoreData.m_aTimestamp, "2021-11-24 19:24:08");
		str_copy(ScoreData.m_aTimestamp, "2021-11-24 19:24:08");
		std::fill(std::begin(ScoreData.m_aCurrentTimeCp), std::end(ScoreData.m_aCurrentTimeCp), 0);
		ASSERT_TRUE(CScoreWorker::SaveTeamScore(m_pConn, &TeamScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;

		str_copy(m_PlayerRequest.m_aMap, "Kobra 3");
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee");
		str_copy(ScoreData.m_aRequestingPlayer, "brainless tee");

		str_copy(ScoreData.m_aName, "nameless tee");
		ScoreData.m_ClientId = 0;
		ASSERT_TRUE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
		str_copy(ScoreData.m_aName, "brainless tee");
		ScoreData.m_ClientId = 1;
		ASSERT_TRUE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
		m_PlayerRequest.m_Offset = 0;
	}
};

TEST_P(TeamScore, All)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_TRUE(CScoreWorker::ShowTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"-------------------------------"});
}

TEST_P(TeamScore, TeamTop5Regional)
{
	g_Config.m_SvRegionalRankings = true;
	str_copy(m_PlayerRequest.m_aServer, "USA");
	ASSERT_TRUE(CScoreWorker::ShowTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"----- USA Team Top -----",
			"1. brainless tee & nameless tee Team Time: 01:40.00"});
}

TEST_P(TeamScore, PlayerExists)
{
	str_copy(m_PlayerRequest.m_aName, "brainless tee");
	ASSERT_TRUE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"---------------------------------"});
}

TEST_P(TeamScore, PlayerDoesntExist)
{
	str_copy(m_PlayerRequest.m_aName, "foo");
	ASSERT_TRUE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"foo has no team ranks"});
}

TEST_P(TeamScore, RankUpdates)
{
	InsertTeamRank(98.0);
	str_copy(m_PlayerRequest.m_aName, "brainless tee");
	ASSERT_TRUE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:38.00",
			"---------------------------------"});
}

// A team of MAX_CLIENTS players with names of maximum length, which is the
// worst case for the chat messages that show team ranks.
struct BigTeamScore : public Score // NOLINT(readability-identifier-naming)
{
	void SetUp() override
	{
		str_copy(g_Config.m_SvSqlServerName, "USA");
		CSqlTeamScoreData TeamScoreData;
		str_copy(TeamScoreData.m_aMap, "Kobra 3");
		str_copy(TeamScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320");
		TeamScoreData.m_Size = MAX_CLIENTS;
		TeamScoreData.m_Time = 100.0f;
		str_copy(TeamScoreData.m_aTimestamp, "2021-11-24 19:24:08");
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			str_format(TeamScoreData.m_aaNames[i], sizeof(TeamScoreData.m_aaNames[i]), "playertee12_%03d", i);
			ASSERT_EQ(str_length(TeamScoreData.m_aaNames[i]), MAX_NAME_LENGTH - 1);
		}
		ASSERT_TRUE(CScoreWorker::SaveTeamScore(m_pConn, &TeamScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;

		CSqlScoreData ScoreData(std::make_shared<CScorePlayerResult>());
		str_copy(ScoreData.m_aMap, "Kobra 3");
		str_copy(ScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320");
		ScoreData.m_Time = 100.0f;
		str_copy(ScoreData.m_aTimestamp, "2021-11-24 19:24:08");
		std::fill(std::begin(ScoreData.m_aCurrentTimeCp), std::end(ScoreData.m_aCurrentTimeCp), 0);
		str_copy(ScoreData.m_aRequestingPlayer, TeamScoreData.m_aaNames[0]);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			str_copy(ScoreData.m_aName, TeamScoreData.m_aaNames[i]);
			ScoreData.m_ClientId = i;
			ASSERT_TRUE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
		}

		str_copy(m_PlayerRequest.m_aMap, "Kobra 3");
		str_copy(m_PlayerRequest.m_aRequestingPlayer, TeamScoreData.m_aaNames[0]);
		str_copy(m_PlayerRequest.m_aName, TeamScoreData.m_aaNames[0]);
		str_copy(m_PlayerRequest.m_aServer, "USA");
		m_PlayerRequest.m_Offset = 0;
	}

	// Messages longer than this are truncated before they reach the client.
	void ExpectMessagesFitIntoChat()
	{
		for(const auto &aMessage : m_pPlayerResult->m_Data.m_aaMessages)
		{
			EXPECT_LT(str_length(aMessage), MAX_CHAT_LENGTH) << aMessage;
		}
	}
};

TEST_P(BigTeamScore, TeamRankShortensNames)
{
	ASSERT_TRUE(CScoreWorker::ShowTeamRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectMessagesFitIntoChat();

	// The names that don't fit into the chat message are summarized.
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[0],
		"1. playertee12_000, playertee12_001, playertee12_002, playertee12_003, "
		"playertee12_004, playertee12_005, playertee12_006, playertee12_007, "
		"playertee12_008, playertee12_009 & 118 more Team time: 01:40.00, "
		"better than 100%, requested by playertee12_000");
}

TEST_P(BigTeamScore, TeamTop5ShortensNames)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_TRUE(CScoreWorker::ShowTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectMessagesFitIntoChat();

	// Five teams can't be wrapped, so the names that don't fit are summarized.
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[1],
		"1. playertee12_000, playertee12_001, playertee12_002, playertee12_003, "
		"playertee12_004, playertee12_005, playertee12_006, playertee12_007, "
		"playertee12_008, playertee12_009, playertee12_010, playertee12_011, "
		"playertee12_012 & 115 more Team Time: 01:40.00");
}

TEST_P(BigTeamScore, PlayerTeamTop5ShortensNames)
{
	ASSERT_TRUE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectMessagesFitIntoChat();

	EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[1],
		"1. playertee12_000, playertee12_001, playertee12_002, playertee12_003, "
		"playertee12_004, playertee12_005, playertee12_006, playertee12_007, "
		"playertee12_008, playertee12_009, playertee12_010, playertee12_011, "
		"playertee12_012 & 115 more Team Time: 01:40.00");
}

struct MapInfo : public Score // NOLINT(readability-identifier-naming)
{
	MapInfo()
	{
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee");
	}
};

TEST_P(MapInfo, ExactNoFinish)
{
	str_copy(m_PlayerRequest.m_aName, "Kobra 3");
	ASSERT_TRUE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released .* ago, 0 finishes by 0 tees"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, ExactFinish)
{
	InsertRank(42.87f);
	str_copy(m_PlayerRequest.m_aRequestingPlayer, "nameless tee");
	str_copy(m_PlayerRequest.m_aName, "Kobra 3");
	ASSERT_TRUE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released .* ago, 1 finish by 1 tee in 00:42 median, your time: 42\\.87"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, Fuzzy)
{
	InsertRank();
	str_copy(m_PlayerRequest.m_aName, "k3");
	ASSERT_TRUE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released .* ago, 1 finish by 1 tee in 01:40 median"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, FuzzyCase)
{
	InsertMap("Reflect", "DarkOort", "Dummy", 20, 3);
	InsertMap("reflects", "Ninjed & Pipou", "Solo", 16, 4);
	str_copy(m_PlayerRequest.m_aName, "reflect");
	ASSERT_TRUE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Reflect\" by DarkOort on Dummy, ★★★✰✰, 20 points, released .* ago, 0 finishes by 0 tees"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, DoesntExit)
{
	str_copy(m_PlayerRequest.m_aName, "f");
	ASSERT_TRUE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"No map like \"f\" found."});
}

struct MapVote : public Score // NOLINT(readability-identifier-naming)
{
	MapVote()
	{
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee");
	}
};

TEST_P(MapVote, Exact)
{
	str_copy(m_PlayerRequest.m_aName, "Kobra 3");
	ASSERT_TRUE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aServer, "novice");
}

TEST_P(MapVote, Fuzzy)
{
	str_copy(m_PlayerRequest.m_aName, "k3");
	ASSERT_TRUE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aServer, "novice");
}

TEST_P(MapVote, FuzzyCase)
{
	InsertMap("Reflect", "DarkOort", "Dummy", 20, 3);
	InsertMap("reflects", "Ninjed & Pipou", "Solo", 16, 4);
	str_copy(m_PlayerRequest.m_aName, "reflect");
	ASSERT_TRUE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aMap, "Reflect");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aServer, "dummy");
}

TEST_P(MapVote, DoesntExist)
{
	str_copy(m_PlayerRequest.m_aName, "f");
	ASSERT_TRUE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"No map like \"f\" found. Try adding a '%' at the start if you don't know the first character. Example: /map %castle for \"Out of Castle\""});
}

struct Points : public Score // NOLINT(readability-identifier-naming)
{
	Points()
	{
		str_copy(m_PlayerRequest.m_aName, "nameless tee");
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee");
		m_PlayerRequest.m_Offset = 0;
	}
};

TEST_P(Points, NoPoints)
{
	ASSERT_TRUE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee has not collected any points so far"});
}

TEST_P(Points, NoPointsTop)
{
	ASSERT_TRUE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"-------- Top Points --------",
					     "-------------------------------"});
}

TEST_P(Points, OnePoints)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	ASSERT_TRUE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"1. nameless tee Points: 2, requested by brainless tee"}, true);
}

TEST_P(Points, OnePointsTop)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	ASSERT_TRUE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"-------- Top Points --------",
			"1. nameless tee Points: 2",
			"-------------------------------"});
}

TEST_P(Points, TwoPoints)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	ASSERT_TRUE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"2. nameless tee Points: 2, requested by brainless tee"}, true);
}

TEST_P(Points, TwoPointsTop)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	ASSERT_TRUE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"-------- Top Points --------",
			"1. brainless tee Points: 3",
			"2. nameless tee Points: 2",
			"-------------------------------"});
}

TEST_P(Points, EqualPoints)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("nameless tee", 1, m_aError, sizeof(m_aError));
	ASSERT_TRUE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"1. nameless tee Points: 3, requested by brainless tee"}, true);
}

TEST_P(Points, EqualPointsTop)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("nameless tee", 1, m_aError, sizeof(m_aError));
	ASSERT_TRUE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"-------- Top Points --------",
			"1. brainless tee Points: 3",
			"1. nameless tee Points: 3",
			"-------------------------------"});
}

struct RandomMap : public Score // NOLINT(readability-identifier-naming)
{
	std::shared_ptr<CScoreRandomMapResult> m_pRandomMapResult{std::make_shared<CScoreRandomMapResult>(0)};
	CSqlRandomMapRequest m_RandomMapRequest{m_pRandomMapResult};

	RandomMap()
	{
		str_copy(m_RandomMapRequest.m_aServerType, "Novice");
		str_copy(m_RandomMapRequest.m_aCurrentMap, "Kobra 4");
		str_copy(m_RandomMapRequest.m_aRequestingPlayer, "nameless tee");
	}
};

TEST_P(RandomMap, NoStars)
{
	m_RandomMapRequest.m_MinStars = -1;
	m_RandomMapRequest.m_MaxStars = -1;
	ASSERT_TRUE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsExists)
{
	m_RandomMapRequest.m_MinStars = 5;
	m_RandomMapRequest.m_MaxStars = 5;
	ASSERT_TRUE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsRangeExists)
{
	m_RandomMapRequest.m_MinStars = 1;
	m_RandomMapRequest.m_MaxStars = 5;
	ASSERT_TRUE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STRNE(m_pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsDoesntExist)
{
	m_RandomMapRequest.m_MinStars = 3;
	m_RandomMapRequest.m_MaxStars = 3;
	ASSERT_TRUE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "No maps found on this server!");
}

TEST_P(RandomMap, UnfinishedExists)
{
	m_RandomMapRequest.m_MinStars = -1;
	m_RandomMapRequest.m_MaxStars = -1;
	ASSERT_TRUE(CScoreWorker::RandomUnfinishedMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, UnfinishedDoesntExist)
{
	InsertRank();
	ASSERT_TRUE(CScoreWorker::RandomUnfinishedMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "nameless tee has no more unfinished maps on this server!");
}

static auto g_pSqliteConn = CreateSqliteConnection(":memory:", true);
#if defined(CONF_TEST_MYSQL)
CMysqlConfig gMysqlConfig{
	"ddnet", // database
	"record", // prefix
	"ddnet", // user
	"thebestpassword", // password
	"localhost", // ip
	"", // bindaddr
	3306, // port
	true, // setup
};
static auto g_pMysqlConn = CreateMysqlConnection(gMysqlConfig);
#endif

static auto g_TestValues{
	testing::Values(
#if defined(CONF_TEST_MYSQL)
		g_pMysqlConn.get(),
#endif
		g_pSqliteConn.get())};

#define INSTANTIATE(SUITE) \
	INSTANTIATE_TEST_SUITE_P(Sql, SUITE, g_TestValues, \
		[](const testing::TestParamInfo<Score::ParamType> &Info) { \
			switch(Info.index) \
			{ \
			case 0: return "SQLite"; \
			case 1: return "MySQL"; \
			default: return "Unknown"; \
			} \
		})

INSTANTIATE(SingleScore);
INSTANTIATE(TeamScore);
INSTANTIATE(BigTeamScore);
INSTANTIATE(MapInfo);
INSTANTIATE(MapVote);
INSTANTIATE(Points);
INSTANTIATE(RandomMap);
