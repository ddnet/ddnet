#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/server/databases/connection.h>
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

int CSaveTeam::FromString(char const *)
{
	// Dummy implementation for testing
	return 1;
}

bool CSaveTeam::MatchPlayers(const char (*paNames)[MAX_NAME_LENGTH], const int *pClientID, int NumPlayer, char *pMessage, int MessageLen)
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
		Init();
		InsertMap();
	}

	~Score()
	{
		conn->Disconnect();
	}

	void Connect()
	{
		ASSERT_FALSE(conn->Connect(aError, sizeof(aError))) << aError;

		// Delete all existing entries for persistent databases like MySQL
		int NumInserted = 0;
		ASSERT_FALSE(conn->PrepareStatement("DELETE FROM record_race;", aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->ExecuteUpdate(&NumInserted, aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->PrepareStatement("DELETE FROM record_teamrace;", aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->ExecuteUpdate(&NumInserted, aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->PrepareStatement("DELETE FROM record_maps;", aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->ExecuteUpdate(&NumInserted, aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->PrepareStatement("DELETE FROM record_points;", aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->ExecuteUpdate(&NumInserted, aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->PrepareStatement("DELETE FROM record_saves;", aError, sizeof(aError))) << aError;
		ASSERT_FALSE(conn->ExecuteUpdate(&NumInserted, aError, sizeof(aError))) << aError;
	}

	void Init()
	{
		CSqlInitData initData(std::make_shared<CScoreInitResult>());
		str_copy(initData.m_aMap, "Kobra 3", sizeof(initData.m_aMap));
		ASSERT_FALSE(CScoreWorker::Init(conn, &initData, aError, sizeof(aError))) << aError;
	}

	void InsertMap()
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"%s into %s_maps(Map, Server, Mapper, Points, Stars, Timestamp) "
			"VALUES (\"Kobra 3\", \"Novice\", \"Zerodin\", 5, 5, \"2015-01-01 00:00:00\");",
			conn->InsertIgnore(), conn->GetPrefix());
		ASSERT_FALSE(conn->PrepareStatement(aBuf, aError, sizeof(aError))) << aError;
		int NumInserted = 0;
		ASSERT_FALSE(conn->ExecuteUpdate(&NumInserted, aError, sizeof(aError))) << aError;
		ASSERT_EQ(NumInserted, 1);
	}

	void InsertRank()
	{
		str_copy(g_Config.m_SvSqlServerName, "USA", sizeof(g_Config.m_SvSqlServerName));
		CSqlScoreData scoreData(std::make_shared<CScorePlayerResult>());
		str_copy(scoreData.m_aMap, "Kobra 3", sizeof(scoreData.m_aMap));
		str_copy(scoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320", sizeof(scoreData.m_aGameUuid));
		str_copy(scoreData.m_aName, "nameless tee", sizeof(scoreData.m_aName));
		scoreData.m_ClientID = 0;
		scoreData.m_Time = 100.0;
		str_copy(scoreData.m_aTimestamp, "2021-11-24 19:24:08", sizeof(scoreData.m_aTimestamp));
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			scoreData.m_aCpCurrent[i] = i;
		str_copy(scoreData.m_aRequestingPlayer, "deen", sizeof(scoreData.m_aRequestingPlayer));
		ASSERT_FALSE(CScoreWorker::SaveScore(conn, &scoreData, false, aError, sizeof(aError))) << aError;
	}

	void ExpectLines(std::shared_ptr<CScorePlayerResult> pPlayerResult, std::initializer_list<const char *> Lines, bool All = false)
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

	IDbConnection *conn{GetParam()};
	char aError[256] = {};
	std::shared_ptr<CScorePlayerResult> pPlayerResult{std::make_shared<CScorePlayerResult>()};
	CSqlPlayerRequest playerRequest{pPlayerResult};
};

struct SingleScore : public Score
{
	SingleScore()
	{
		InsertRank();
		str_copy(playerRequest.m_aMap, "Kobra 3", sizeof(playerRequest.m_aMap));
		str_copy(playerRequest.m_aRequestingPlayer, "brainless tee", sizeof(playerRequest.m_aRequestingPlayer));
		playerRequest.m_Offset = 0;
		str_copy(playerRequest.m_aServer, "GER", sizeof(playerRequest.m_aServer));
		str_copy(playerRequest.m_aName, "nameless tee", sizeof(playerRequest.m_aMap));
	}
};

TEST_P(SingleScore, Top)
{
	ASSERT_FALSE(CScoreWorker::ShowTop(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"------------ GER Top ------------"});
}

TEST_P(SingleScore, Rank)
{
	ASSERT_FALSE(CScoreWorker::ShowRank(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1 - GER unranked"}, true);
}

TEST_P(SingleScore, TopServer)
{
	str_copy(playerRequest.m_aServer, "USA", sizeof(playerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowTop(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"------------ Global Top ------------",
			"1. nameless tee Time: 01:40.00",
			"---------------------------------------"});
}

TEST_P(SingleScore, RankServer)
{
	str_copy(playerRequest.m_aServer, "USA", sizeof(playerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowRank(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"nameless tee - 01:40.00 - better than 100% - requested by brainless tee", "Global rank 1 - USA rank 1"}, true);
}

TEST_P(SingleScore, TimesExists)
{
	ASSERT_FALSE(CScoreWorker::ShowTimes(conn, &playerRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[0], "------------- Last Times -------------");
	char aBuf[128];
	str_copy(aBuf, pPlayerResult->m_Data.m_aaMessages[1], 7);
	EXPECT_STREQ(aBuf, "[USA] ");

	str_copy(aBuf, pPlayerResult->m_Data.m_aaMessages[1] + str_length(pPlayerResult->m_Data.m_aaMessages[1]) - 10, 11);
	EXPECT_STREQ(aBuf, ", 01:40.00");
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[2], "----------------------------------------------------");
	for(int i = 3; i < CScorePlayerResult::MAX_MESSAGES; i++)
	{
		EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[i], "");
	}
}

TEST_P(SingleScore, TimesDoesntExist)
{
	str_copy(playerRequest.m_aName, "foo", sizeof(playerRequest.m_aMap));
	ASSERT_FALSE(CScoreWorker::ShowTimes(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"There are no times in the specified range"});
}

struct TeamScore : public Score
{
	void SetUp()
	{
		CSqlTeamScoreData teamScoreData;
		str_copy(teamScoreData.m_aMap, "Kobra 3", sizeof(teamScoreData.m_aMap));
		str_copy(teamScoreData.m_aGameUuid, "8d300ecf-5873-4297-bee5-95668fdff320", sizeof(teamScoreData.m_aGameUuid));
		teamScoreData.m_Size = 2;
		str_copy(teamScoreData.m_aaNames[0], "nameless tee", sizeof(teamScoreData.m_aaNames[0]));
		str_copy(teamScoreData.m_aaNames[1], "brainless tee", sizeof(teamScoreData.m_aaNames[1]));
		teamScoreData.m_Time = 100.0;
		str_copy(teamScoreData.m_aTimestamp, "2021-11-24 19:24:08", sizeof(teamScoreData.m_aTimestamp));
		ASSERT_FALSE(CScoreWorker::SaveTeamScore(conn, &teamScoreData, false, aError, sizeof(aError))) << aError;

		str_copy(playerRequest.m_aMap, "Kobra 3", sizeof(playerRequest.m_aMap));
		str_copy(playerRequest.m_aRequestingPlayer, "brainless tee", sizeof(playerRequest.m_aRequestingPlayer));
		playerRequest.m_Offset = 0;
	}
};

TEST_P(TeamScore, All)
{
	ASSERT_FALSE(CScoreWorker::ShowTeamTop5(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"-------------------------------"});
}

TEST_P(TeamScore, PlayerExists)
{
	str_copy(playerRequest.m_aName, "brainless tee", sizeof(playerRequest.m_aMap));
	ASSERT_FALSE(CScoreWorker::ShowPlayerTeamTop5(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"------- Team Top 5 -------",
			"1. brainless tee & nameless tee Team Time: 01:40.00",
			"-------------------------------"});
}

TEST_P(TeamScore, PlayerDoesntExist)
{
	str_copy(playerRequest.m_aName, "foo", sizeof(playerRequest.m_aMap));
	ASSERT_FALSE(CScoreWorker::ShowPlayerTeamTop5(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"foo has no team ranks"});
}

struct MapInfo : public Score
{
	MapInfo()
	{
		str_copy(playerRequest.m_aRequestingPlayer, "brainless tee", sizeof(playerRequest.m_aRequestingPlayer));
	}
};

TEST_P(MapInfo, ExactNoFinish)
{
	str_copy(playerRequest.m_aName, "Kobra 3", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released 6 years and 11 months ago, 0 finishes by 0 tees"});
}

TEST_P(MapInfo, ExactFinish)
{
	InsertRank();
	str_copy(playerRequest.m_aName, "Kobra 3", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released 6 years and 11 months ago, 1 finish by 1 tee in 01:40 median"});
}

TEST_P(MapInfo, Fuzzy)
{
	InsertRank();
	str_copy(playerRequest.m_aName, "k3", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"\"Kobra 3\" by Zerodin on Novice, ★★★★★, 5 points, released 6 years and 11 months ago, 1 finish by 1 tee in 01:40 median"});
}

TEST_P(MapInfo, DoesntExit)
{
	str_copy(playerRequest.m_aName, "f", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapInfo(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"No map like \"f\" found."});
}

struct MapVote : public Score
{
	MapVote()
	{
		str_copy(playerRequest.m_aRequestingPlayer, "brainless tee", sizeof(playerRequest.m_aRequestingPlayer));
	}
};

TEST_P(MapVote, Exact)
{
	str_copy(playerRequest.m_aName, "Kobra 3", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapVote(conn, &playerRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(pPlayerResult->m_Data.m_MapVote.m_aMap, "Kobra 3");
	EXPECT_STREQ(pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(pPlayerResult->m_Data.m_MapVote.m_aServer, "novice");
}

TEST_P(MapVote, Fuzzy)
{
	str_copy(playerRequest.m_aName, "k3", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapVote(conn, &playerRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pPlayerResult->m_MessageKind, CScorePlayerResult::MAP_VOTE);
	EXPECT_STREQ(pPlayerResult->m_Data.m_MapVote.m_aMap, "Kobra 3");
	EXPECT_STREQ(pPlayerResult->m_Data.m_MapVote.m_aReason, "/map");
	EXPECT_STREQ(pPlayerResult->m_Data.m_MapVote.m_aServer, "novice");
}

TEST_P(MapVote, DoesntExist)
{
	str_copy(playerRequest.m_aName, "f", sizeof(playerRequest.m_aName));
	ASSERT_FALSE(CScoreWorker::MapVote(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"No map like \"f\" found. Try adding a '%' at the start if you don't know the first character. Example: /map %castle for \"Out of Castle\""});
}

struct Points : public Score
{
	Points()
	{
		str_copy(playerRequest.m_aName, "nameless tee", sizeof(playerRequest.m_aName));
		str_copy(playerRequest.m_aRequestingPlayer, "brainless tee", sizeof(playerRequest.m_aRequestingPlayer));
		playerRequest.m_Offset = 0;
	}
};

TEST_P(Points, NoPoints)
{
	ASSERT_FALSE(CScoreWorker::ShowPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"nameless tee has not collected any points so far"});
}

TEST_P(Points, NoPointsTop)
{
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"-------- Top Points --------",
					   "-------------------------------"});
}

TEST_P(Points, OnePoints)
{
	conn->AddPoints("nameless tee", 2, aError, sizeof(aError));
	ASSERT_FALSE(CScoreWorker::ShowPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"1. nameless tee Points: 2, requested by brainless tee"}, true);
}

TEST_P(Points, OnePointsTop)
{
	conn->AddPoints("nameless tee", 2, aError, sizeof(aError));
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"-------- Top Points --------",
			"1. nameless tee Points: 2",
			"-------------------------------"});
}

TEST_P(Points, TwoPoints)
{
	conn->AddPoints("nameless tee", 2, aError, sizeof(aError));
	conn->AddPoints("brainless tee", 3, aError, sizeof(aError));
	ASSERT_FALSE(CScoreWorker::ShowPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"2. nameless tee Points: 2, requested by brainless tee"}, true);
}

TEST_P(Points, TwoPointsTop)
{
	conn->AddPoints("nameless tee", 2, aError, sizeof(aError));
	conn->AddPoints("brainless tee", 3, aError, sizeof(aError));
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"-------- Top Points --------",
			"1. brainless tee Points: 3",
			"2. nameless tee Points: 2",
			"-------------------------------"});
}

TEST_P(Points, EqualPoints)
{
	conn->AddPoints("nameless tee", 2, aError, sizeof(aError));
	conn->AddPoints("brainless tee", 3, aError, sizeof(aError));
	conn->AddPoints("nameless tee", 1, aError, sizeof(aError));
	ASSERT_FALSE(CScoreWorker::ShowPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult, {"1. nameless tee Points: 3, requested by brainless tee"}, true);
}

TEST_P(Points, EqualPointsTop)
{
	conn->AddPoints("nameless tee", 2, aError, sizeof(aError));
	conn->AddPoints("brainless tee", 3, aError, sizeof(aError));
	conn->AddPoints("nameless tee", 1, aError, sizeof(aError));
	ASSERT_FALSE(CScoreWorker::ShowTopPoints(conn, &playerRequest, aError, sizeof(aError))) << aError;
	ExpectLines(pPlayerResult,
		{"-------- Top Points --------",
			"1. brainless tee Points: 3",
			"1. nameless tee Points: 3",
			"-------------------------------"});
}

struct RandomMap : public Score
{
	std::shared_ptr<CScoreRandomMapResult> pRandomMapResult{std::make_shared<CScoreRandomMapResult>(0)};
	CSqlRandomMapRequest randomMapRequest{pRandomMapResult};

	RandomMap()
	{
		str_copy(randomMapRequest.m_aServerType, "Novice", sizeof(randomMapRequest.m_aServerType));
		str_copy(randomMapRequest.m_aCurrentMap, "Kobra 4", sizeof(randomMapRequest.m_aCurrentMap));
		str_copy(randomMapRequest.m_aRequestingPlayer, "nameless tee", sizeof(randomMapRequest.m_aRequestingPlayer));
	}
};

TEST_P(RandomMap, NoStars)
{
	randomMapRequest.m_Stars = -1;
	ASSERT_FALSE(CScoreWorker::RandomMap(conn, &randomMapRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pRandomMapResult->m_ClientID, 0);
	EXPECT_STREQ(pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsExists)
{
	randomMapRequest.m_Stars = 5;
	ASSERT_FALSE(CScoreWorker::RandomMap(conn, &randomMapRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pRandomMapResult->m_ClientID, 0);
	EXPECT_STREQ(pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, StarsDoesntExist)
{
	randomMapRequest.m_Stars = 3;
	ASSERT_FALSE(CScoreWorker::RandomMap(conn, &randomMapRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pRandomMapResult->m_ClientID, 0);
	EXPECT_STREQ(pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(pRandomMapResult->m_aMessage, "No maps found on this server!");
}

TEST_P(RandomMap, UnfinishedExists)
{
	randomMapRequest.m_Stars = -1;
	ASSERT_FALSE(CScoreWorker::RandomUnfinishedMap(conn, &randomMapRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pRandomMapResult->m_ClientID, 0);
	EXPECT_STREQ(pRandomMapResult->m_aMap, "Kobra 3");
	EXPECT_STREQ(pRandomMapResult->m_aMessage, "");
}

TEST_P(RandomMap, UnfinishedDoesntExist)
{
	InsertRank();
	ASSERT_FALSE(CScoreWorker::RandomUnfinishedMap(conn, &randomMapRequest, aError, sizeof(aError))) << aError;
	EXPECT_EQ(pRandomMapResult->m_ClientID, 0);
	EXPECT_STREQ(pRandomMapResult->m_aMap, "");
	EXPECT_STREQ(pRandomMapResult->m_aMessage, "You have no more unfinished maps on this server!");
}

auto testValues
{
	testing::Values(CreateSqliteConnection(":memory:", true)
#if defined(CONF_TEST_MYSQL)
				,
		CreateMysqlConnection("ddnet", "record", "ddnet", "thebestpassword", "localhost", 3306, true)
#endif
	)
};

#define INSTANTIATE(SUITE) \
	INSTANTIATE_TEST_SUITE_P(Sql, SUITE, testValues, \
		[](const testing::TestParamInfo<Score::ParamType> &info) { \
			switch(info.index) \
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
