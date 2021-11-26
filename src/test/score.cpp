#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/server/databases/connection.h>
#include <engine/shared/config.h>
#include <game/server/scoreworker.h>
#include <iostream>

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

class Score : public ::testing::Test
{
public:
	~Score()
	{
		delete conn;
	}

protected:
	IDbConnection *conn{CreateSqliteConnection(":memory:", true)};
	char aError[256] = {};
};

TEST_F(Score, SaveScore)
{
	ASSERT_FALSE(conn->Connect(aError, sizeof(aError))) << aError;

	CSqlInitData initData(std::make_shared<CScoreInitResult>());
	str_copy(initData.m_aMap, "Kobra 3", sizeof(initData.m_aMap));
	ASSERT_FALSE(CScoreWorker::Init(conn, &initData, aError, sizeof(aError))) << aError;

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
	scoreData.m_Num = 1;
	scoreData.m_Search = false;
	str_copy(scoreData.m_aRequestingPlayer, "deen", sizeof(scoreData.m_aRequestingPlayer));
	ASSERT_FALSE(CScoreWorker::SaveScore(conn, &scoreData, false, aError, sizeof(aError))) << aError;

	auto pPlayerResult = std::make_shared<CScorePlayerResult>();
	CSqlPlayerRequest playerRequest(pPlayerResult);
	str_copy(playerRequest.m_aName, "Kobra 3", sizeof(playerRequest.m_aName));
	str_copy(playerRequest.m_aMap, "Kobra 3", sizeof(playerRequest.m_aMap));
	str_copy(playerRequest.m_aRequestingPlayer, "brainless tee", sizeof(playerRequest.m_aRequestingPlayer));
	playerRequest.m_Offset = 0;
	str_copy(playerRequest.m_aServer, "GER", sizeof(playerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowTop(conn, &playerRequest, aError, sizeof(aError))) << aError;

	ASSERT_EQ(pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[0], "------------ Global Top ------------");
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[1], "1. nameless tee Time: 01:40.00");
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[2], "------------ GER Top ------------");
	for(int i = 3; i < CScorePlayerResult::MAX_MESSAGES; i++)
		EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[i], "");

	str_copy(playerRequest.m_aServer, "USA", sizeof(playerRequest.m_aServer));
	ASSERT_FALSE(CScoreWorker::ShowTop(conn, &playerRequest, aError, sizeof(aError))) << aError;

	ASSERT_EQ(pPlayerResult->m_MessageKind, CScorePlayerResult::DIRECT);
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[0], "------------ Global Top ------------");
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[1], "1. nameless tee Time: 01:40.00");
	EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[2], "---------------------------------------");
	for(int i = 3; i < CScorePlayerResult::MAX_MESSAGES; i++)
		EXPECT_STREQ(pPlayerResult->m_Data.m_aaMessages[i], "");
}
