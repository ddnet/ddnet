#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/shared/config.h>
#include <game/server/scoreworker.h>

#include <sqlite3.h>

#if defined(CONF_TEST_MYSQL)
int DummyMysqlInit = (MysqlInit(), 1);
#endif

char *CSaveTeam::GetString()
{
	// Dummy implementation for testing
	return nullptr;
}

int CSaveTeam::FromString(const char *)
{
	// Dummy implementation for testing
	return 1;
}

bool CSaveTeam::MatchPlayers(const char (*paNames)[MAX_NAME_LENGTH], const int *pClientId, int NumPlayer, char *pMessage, int MessageLen) const
{
	// Dummy implementation for testing
	return false;
}

TEST(SQLite, Version)
{
	ASSERT_GE(sqlite3_libversion_number(), 3025000) << "SQLite >= 3.25.0 required for Window functions";
}

struct Score : public testing::TestWithParam<IDbConnection *>
{
	Score()
	{
		Connect();
		LoadBestTime();
		InsertMap();
	}

	~Score()
	{
		m_pConn->Disconnect();
	}

	void Connect()
	{
		ASSERT_FALSE(m_pConn->Connect(m_aError, sizeof(m_aError))) << m_aError;

		// Delete all existing entries for persistent databases like MySQL
		int NumInserted = 0;
		ASSERT_FALSE(m_pConn->PrepareStatement("DELETE FROM record_race", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->PrepareStatement("DELETE FROM record_teamrace", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->PrepareStatement("DELETE FROM record_maps", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->PrepareStatement("DELETE FROM record_points", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->PrepareStatement("DELETE FROM record_saves", m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_FALSE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
	}

	void LoadBestTime()
	{
		CSqlLoadBestTimeRequest loadBestTimeReq(std::make_shared<CScoreLoadBestTimeResult>());
		str_copy(loadBestTimeReq.m_aMap, "Kobra 3", sizeof(loadBestTimeReq.m_aMap));
		ASSERT_FALSE(CScoreWorker::LoadBestTime(m_pConn, &loadBestTimeReq, m_aError, sizeof(m_aError))) << m_aError;
	}

	void InsertMap()
	{
		char aTimestamp[32];
		str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"%s into %s_maps(Map, Server, Mapper, Points, Stars, Timestamp) "
			"VALUES (\"Kobra 3\", \"Novice\", \"Zerodin\", 5, 5, %s)",
			m_pConn->InsertIgnore(), m_pConn->GetPrefix(), m_pConn->InsertTimestampAsUtc());
		ASSERT_FALSE(m_pConn->PrepareStatement(aBuf, m_aError, sizeof(m_aError))) << m_aError;
		m_pConn->BindString(1, aTimestamp);
		int NumInserted = 0;
		ASSERT_FALSE(m_pConn->ExecuteUpdate(&NumInserted, m_aError, sizeof(m_aError))) << m_aError;
		ASSERT_EQ(NumInserted, 1);
	}

	void InsertRank(float Time = 100.0, bool WithTimeCheckPoints = false)
	{
		str_copy(g_Config.m_SvSqlServerName, "USA", sizeof(g_Config.m_SvSqlServerName));
		CSqlScoreData ScoreData(std::make_shared<CScorePlayerResult>());
		str_copy(ScoreData.m_aMap, "Kobra 3", sizeof(ScoreData.m_aMap));
		str_copy(ScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320", sizeof(ScoreData.m_aGameUuid));
		str_copy(ScoreData.m_aName, "nameless tee", sizeof(ScoreData.m_aName));
		ScoreData.m_ClientId = 0;
		ScoreData.m_Time = Time;
		str_copy(ScoreData.m_aTimestamp, "2021-11-24 19:24:08", sizeof(ScoreData.m_aTimestamp));
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			ScoreData.m_aCurrentTimeCp[i] = WithTimeCheckPoints ? i : 0;
		str_copy(ScoreData.m_aRequestingPlayer, "deen", sizeof(ScoreData.m_aRequestingPlayer));
		ASSERT_FALSE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
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

struct SingleScore : public Score
{
	SingleScore()
	{
		InsertRank();
		str_copy(m_PlayerRequest.m_aMap, "Kobra 3", sizeof(m_PlayerRequest.m_aMap));
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
		m_PlayerRequest.m_Offset = 0;
		str_copy(m_PlayerRequest.m_aServer, "GER", sizeof(m_PlayerRequest.m_aServer));
		str_copy(m_PlayerRequest.m_aName, "nameless tee", sizeof(m_PlayerRequest.m_aMap));
	}
};

TEST_P(SingleScore, TopRegional)
{
	g_Config.m_SvRegionalRankings = true;
	ASSERT_FALSE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"------------ GER Top ------------"});
}

TEST_P(SingleScore, Top)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_FALSE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"-----------------------------------------"});
}

TEST_P(SingleScore, RankRegional)
{
	g_Config.m_SvRegionalRankings = true;
	ASSERT_FALSE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1 - GER unranked"}, true);
}

TEST_P(SingleScore, Rank)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_FALSE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1"}, true);
}

TEST_P(SingleScore, TopServerRegional)
{
	g_Config.m_SvRegionalRankings = true;
	str_copy(m_PlayerRequest.m_aServer, "USA", sizeof(m_PlayerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"------------ USA Top ------------",
			"1. nameless tee Time: 01:40.00"});
}

TEST_P(SingleScore, TopServer)
{
	g_Config.m_SvRegionalRankings = false;
	str_copy(m_PlayerRequest.m_aServer, "USA", sizeof(m_PlayerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowTop(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"-----------------------------------------"});
}

TEST_P(SingleScore, RankServerRegional)
{
	g_Config.m_SvRegionalRankings = true;
	str_copy(m_PlayerRequest.m_aServer, "USA", sizeof(m_PlayerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1 - USA rank 1"}, true);
}

TEST_P(SingleScore, RankServer)
{
	g_Config.m_SvRegionalRankings = false;
	str_copy(m_PlayerRequest.m_aServer, "USA", sizeof(m_PlayerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowRank(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1"}, true);
}

TEST_P(SingleScore, LoadPlayerData)
{
	InsertRank(120.0, true);
	str_copy(m_PlayerRequest.m_aName, "", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	ASSERT_FALSE(CScoreWorker::LoadPlayerData(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::PLAYER_INFO);
	ASSERT_FALSE(m_pPlayerResult->m_Data.m_Info.m_Time.has_value());
	for(auto &Time : m_pPlayerResult->m_Data.m_Info.m_aTimeCp)
	{
		ASSERT_EQ(Time, 0);
	}

	str_copy(m_PlayerRequest.m_aRequestingPlayer, "nameless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	str_copy(m_PlayerRequest.m_aName, "", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	ASSERT_FALSE(CScoreWorker::LoadPlayerData(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::PLAYER_INFO);
	ASSERT_TRUE(m_pPlayerResult->m_Data.m_Info.m_Time.has_value());
	ASSERT_EQ(*m_pPlayerResult->m_Data.m_Info.m_Time, 100.0);
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
	{
		ASSERT_EQ(m_pPlayerResult->m_Data.m_Info.m_aTimeCp[i], i);
	}

	str_copy(m_PlayerRequest.m_aRequestingPlayer, "finishless", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	str_copy(m_PlayerRequest.m_aName, "nameless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	ASSERT_FALSE(CScoreWorker::LoadPlayerData(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::PLAYER_INFO);
	ASSERT_FALSE(m_pPlayerResult->m_Data.m_Info.m_Time.has_value());
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
	{
		ASSERT_EQ(m_pPlayerResult->m_Data.m_Info.m_aTimeCp[i], i);
	}
}

TEST_P(SingleScore, TimesExists)
{
	ASSERT_FALSE(CScoreWorker::ShowTimes(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
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
	ASSERT_FALSE(CScoreWorker::ShowTimes(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"There are no times in the specified range"});
}

struct TeamScore : public Score
{
	void SetUp() override
	{
		InsertTeamRank(100.0);
	}

	void InsertTeamRank(float Time = 100.0)
	{
		str_copy(g_Config.m_SvSqlServerName, "USA", sizeof(g_Config.m_SvSqlServerName));
		CSqlTeamScoreData teamScoreData;
		CSqlScoreData ScoreData(std::make_shared<CScorePlayerResult>());
		str_copy(teamScoreData.m_aMap, "Kobra 3", sizeof(teamScoreData.m_aMap));
		str_copy(ScoreData.m_aMap, "Kobra 3", sizeof(ScoreData.m_aMap));
		str_copy(teamScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320", sizeof(teamScoreData.m_aGameUuid));
		str_copy(ScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320", sizeof(ScoreData.m_aGameUuid));
		teamScoreData.m_Size = 2;
		str_copy(teamScoreData.m_aaNames[0], "nameless tee", sizeof(teamScoreData.m_aaNames[0]));
		str_copy(teamScoreData.m_aaNames[1], "brainless tee", sizeof(teamScoreData.m_aaNames[1]));
		teamScoreData.m_Time = Time;
		ScoreData.m_Time = Time;
		str_copy(teamScoreData.m_aTimestamp, "2021-11-24 19:24:08", sizeof(teamScoreData.m_aTimestamp));
		str_copy(ScoreData.m_aTimestamp, "2021-11-24 19:24:08", sizeof(ScoreData.m_aTimestamp));
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			ScoreData.m_aCurrentTimeCp[i] = 0;
		ASSERT_FALSE(CScoreWorker::SaveTeamScore(m_pConn, &teamScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;

		str_copy(m_PlayerRequest.m_aMap, "Kobra 3", sizeof(m_PlayerRequest.m_aMap));
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
		str_copy(ScoreData.m_aRequestingPlayer, "brainless tee", sizeof(ScoreData.m_aRequestingPlayer));

		str_copy(ScoreData.m_aName, "nameless tee", sizeof(ScoreData.m_aName));
		ScoreData.m_ClientId = 0;
		ASSERT_FALSE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
		str_copy(ScoreData.m_aName, "brainless tee", sizeof(ScoreData.m_aName));
		ScoreData.m_ClientId = 1;
		ASSERT_FALSE(CScoreWorker::SaveScore(m_pConn, &ScoreData, Write::NORMAL, m_aError, sizeof(m_aError))) << m_aError;
		m_PlayerRequest.m_Offset = 0;
	}
};

TEST_P(TeamScore, All)
{
	g_Config.m_SvRegionalRankings = false;
	ASSERT_FALSE(CScoreWorker::ShowTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"-------------------------------"});
}

TEST_P(TeamScore, TeamTop5Regional)
{
	g_Config.m_SvRegionalRankings = true;
	str_copy(m_PlayerRequest.m_aServer, "USA", sizeof(m_PlayerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"----- USA Team Top -----",
			"1. brainless tee & nameless tee Team Time: 01:40.00"});
}

TEST_P(TeamScore, PlayerExists)
{
	str_copy(m_PlayerRequest.m_aName, "brainless tee", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"---------------------------------"});
}

TEST_P(TeamScore, PlayerDoesntExist)
{
	str_copy(m_PlayerRequest.m_aName, "foo", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"foo has no team ranks"});
}

TEST_P(TeamScore, RankUpdates)
{
	InsertTeamRank(98.0);
	str_copy(m_PlayerRequest.m_aName, "brainless tee", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::ShowPlayerTeamTop5(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:38.00",
			"---------------------------------"});
}

struct MapInfo : public Score
{
	MapInfo()
	{
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	}
};

TEST_P(MapInfo, ExactNoFinish)
{
	str_copy(m_PlayerRequest.m_aName, "Kobra 3", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released .* ago, 0 finishes by 0 tees"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, ExactFinish)
{
	InsertRank();
	str_copy(m_PlayerRequest.m_aName, "Kobra 3", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released .* ago, 1 finish by 1 tee in 01:40 median"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, Fuzzy)
{
	InsertRank();
	str_copy(m_PlayerRequest.m_aName, "k3", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;

	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_THAT(m_pPlayerResult->m_Data.m_aaMessages[0], testing::MatchesRegex("\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released .* ago, 1 finish by 1 tee in 01:40 median"));
	for(int i = 1; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(m_pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(MapInfo, DoesntExit)
{
	str_copy(m_PlayerRequest.m_aName, "f", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"No map like \"f\" found."});
}

struct MapVote : public Score
{
	MapVote()
	{
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
	}
};

TEST_P(MapVote, Exact)
{
	str_copy(m_PlayerRequest.m_aName, "Kobra 3", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aServer, "novice");
}

TEST_P(MapVote, Fuzzy)
{
	str_copy(m_PlayerRequest.m_aName, "k3", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(m_pPlayerResult->m_Data.m_MapVote.m_aServer, "novice");
}

TEST_P(MapVote, DoesntExist)
{
	str_copy(m_PlayerRequest.m_aName, "f", sizeof(m_PlayerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapVote(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"No map like \"f\" found. Try adding a '%' at the start if you don't know the first character. Example: /map %castle for \"Out of Castle\""});
}

struct Points : public Score
{
	Points()
	{
		str_copy(m_PlayerRequest.m_aName, "nameless tee", sizeof(m_PlayerRequest.m_aName));
		str_copy(m_PlayerRequest.m_aRequestingPlayer, "brainless tee", sizeof(m_PlayerRequest.m_aRequestingPlayer));
		m_PlayerRequest.m_Offset = 0;
	}
};

TEST_P(Points, NoPoints)
{
	ASSERT_FALSE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"nameless tee has not collected any points so far"});
}

TEST_P(Points, NoPointsTop)
{
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"-------- Top Points --------",
					     "-------------------------------"});
}

TEST_P(Points, OnePoints)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	ASSERT_FALSE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"1. nameless tee Points: 2, requested by brainless tee"}, true);
}

TEST_P(Points, OnePointsTop)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"-------- Top Points --------",
			"1. nameless tee Points: 2",
			"-------------------------------"});
}

TEST_P(Points, TwoPoints)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	ASSERT_FALSE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"2. nameless tee Points: 2, requested by brainless tee"}, true);
}

TEST_P(Points, TwoPointsTop)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
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
	ASSERT_FALSE(CScoreWorker::ShowPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult, {"1. nameless tee Points: 3, requested by brainless tee"}, true);
}

TEST_P(Points, EqualPointsTop)
{
	m_pConn->AddPoints("nameless tee", 2, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("brainless tee", 3, m_aError, sizeof(m_aError));
	m_pConn->AddPoints("nameless tee", 1, m_aError, sizeof(m_aError));
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(m_pConn, &m_PlayerRequest, m_aError, sizeof(m_aError))) << m_aError;
	ExpectLines(m_pPlayerResult,
		{"-------- Top Points --------",
			"1. brainless tee Points: 3",
			"1. nameless tee Points: 3",
			"-------------------------------"});
}

struct RandomMap : public Score
{
	std::shared_ptr<CScoreRandomMapResult> m_pRandomMapResult{std::make_shared<CScoreRandomMapResult>(0)};
	CSqlRandomMapRequest m_RandomMapRequest{m_pRandomMapResult};

	RandomMap()
	{
		str_copy(m_RandomMapRequest.m_aServerType, "Novice", sizeof(m_RandomMapRequest.m_aServerType));
		str_copy(m_RandomMapRequest.m_aCurrentMap, "Kobra 4", sizeof(m_RandomMapRequest.m_aCurrentMap));
		str_copy(m_RandomMapRequest.m_aRequestingPlayer, "nameless tee", sizeof(m_RandomMapRequest.m_aRequestingPlayer));
	}
};

TEST_P(RandomMap, NoStars)
{
	m_RandomMapRequest.m_Stars = -1;
	ASSERT_FALSE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsExists)
{
	m_RandomMapRequest.m_Stars = 5;
	ASSERT_FALSE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsDoesntExist)
{
	m_RandomMapRequest.m_Stars = 3;
	ASSERT_FALSE(CScoreWorker::RandomMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "No maps found on this server!");
}

TEST_P(RandomMap, UnfinishedExists)
{
	m_RandomMapRequest.m_Stars = -1;
	ASSERT_FALSE(CScoreWorker::RandomUnfinishedMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, UnfinishedDoesntExist)
{
	InsertRank();
	ASSERT_FALSE(CScoreWorker::RandomUnfinishedMap(m_pConn, &m_RandomMapRequest, m_aError, sizeof(m_aError))) << m_aError;
	EXPECT_EQ(m_pRandomMapResult->m_ClientId, 0);
	EXPECT_STREQ(m_pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(m_pRandomMapResult->m_aMessage, "nameless tee has no more unfinished maps on this server!");
}

auto g_pSqliteConn = CreateSqliteConnection(":memory:", true);
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
auto g_pMysqlConn = CreateMysqlConnection(gMysqlConfig);
#endif

auto g_TestValues
{
	testing::Values(
#if defined(CONF_TEST_MYSQL)
		g_pMysqlConn.get(),
#endif
		g_pSqliteConn.get())
};

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
INSTANTIATE(MapInfo);
INSTANTIATE(MapVote);
INSTANTIATE(Points);
INSTANTIATE(RandomMap);
