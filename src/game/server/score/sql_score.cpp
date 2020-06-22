/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore class by Sushi */
#if defined(CONF_SQL)
#include "sql_score.h"

#include <fstream>
#include <cstring>
#include <random>
#include <cppconn/prepared_statement.h>

#include <base/system.h>
#include <engine/server/sql_connector.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <algorithm>

#include "../entities/character.h"
#include "../gamemodes/DDRace.h"
#include "../save.h"

std::atomic_int CSqlScore::ms_InstanceCount(0);

CSqlPlayerResult::CSqlPlayerResult() :
	m_Done(false)
{
	SetVariant(Variant::DIRECT);
}

void CSqlPlayerResult::SetVariant(Variant v)
{
	m_MessageKind = v;
	switch(v)
	{
	case DIRECT:
	case ALL:
		for(int i = 0; i < MAX_MESSAGES; i++)
			m_Data.m_aaMessages[i][0] = 0;
		break;
	case BROADCAST:
		m_Data.m_Broadcast[0] = 0;
		break;
	case MAP_VOTE:
		m_Data.m_MapVote.m_Map[0] = '\0';
		m_Data.m_MapVote.m_Reason[0] = '\0';
		m_Data.m_MapVote.m_Server[0] = '\0';
		break;
	case PLAYER_INFO:
		m_Data.m_Info.m_Score = -9999;
		m_Data.m_Info.m_Birthday = 0;
		m_Data.m_Info.m_HasFinishScore = false;
		m_Data.m_Info.m_Time = 0;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_Data.m_Info.m_CpTime[i] = 0;
	}
}

template < typename TResult >
CSqlExecData<TResult>::CSqlExecData(
		bool (*pFuncPtr) (CSqlServer*, const CSqlData<TResult> *, bool),
		CSqlData<TResult> *pSqlResult,
		bool ReadOnly
) :
	m_pFuncPtr(pFuncPtr),
	m_pSqlData(pSqlResult),
	m_ReadOnly(ReadOnly)
{
	++CSqlScore::ms_InstanceCount;
}

template < typename TResult >
CSqlExecData<TResult>::~CSqlExecData()
{
	--CSqlScore::ms_InstanceCount;
}

std::shared_ptr<CSqlPlayerResult> CSqlScore::NewSqlPlayerResult(int ClientID)
{
	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientID];
	if(pCurPlayer->m_SqlQueryResult != nullptr) // TODO: send player a message: "too many requests"
		return nullptr;
	pCurPlayer->m_SqlQueryResult = std::make_shared<CSqlPlayerResult>();
	return pCurPlayer->m_SqlQueryResult;
}

void CSqlScore::ExecPlayerThread(
		bool (*pFuncPtr) (CSqlServer*, const CSqlData<CSqlPlayerResult> *, bool),
		const char* pThreadName,
		int ClientID,
		const char* pName,
		int Offset
) {
	auto pResult = NewSqlPlayerResult(ClientID);
	if(pResult == nullptr)
		return;
	CSqlPlayerRequest *Tmp = new CSqlPlayerRequest(pResult);
	Tmp->m_Name = pName;
	Tmp->m_Map = g_Config.m_SvMap;
	Tmp->m_RequestingPlayer = Server()->ClientName(ClientID);
	Tmp->m_Offset = Offset;

	thread_init_and_detach(CSqlExecData<CSqlPlayerResult>::ExecSqlFunc,
			new CSqlExecData<CSqlPlayerResult>(pFuncPtr, Tmp),
			pThreadName);
}

void CSqlScore::GeneratePassphrase(char *pBuf, int BufSize)
{
	for(int i = 0; i < 3; i++)
	{
		if(i != 0)
			str_append(pBuf, " ", BufSize);
		// TODO: decide if the slight bias towards lower numbers is ok
		int Rand = m_Prng.RandomBits() % m_aWordlist.size();
		str_append(pBuf, m_aWordlist[Rand].c_str(), BufSize);
	}
}

LOCK CSqlScore::ms_FailureFileLock = lock_create();

void CSqlScore::OnShutdown()
{
	int i = 0;
	while (CSqlScore::ms_InstanceCount != 0)
	{
		if (i > 600)  {
			dbg_msg("sql", "Waited 60 seconds for score-threads to complete, quitting anyway");
			break;
		}

		// print a log about every two seconds
		if (i % 20 == 0)
			dbg_msg("sql", "Waiting for score-threads to complete (%d left)", CSqlScore::ms_InstanceCount.load());
		++i;
		thread_sleep(100000);
	}

	lock_destroy(ms_FailureFileLock);
}

template < typename TResult >
void CSqlExecData<TResult>::ExecSqlFunc(void *pUser)
{
	CSqlExecData<TResult>* pData = (CSqlExecData<TResult> *)pUser;

	CSqlConnector connector;

	bool Success = false;

	try {
		// try to connect to a working database server
		while (!Success && !connector.MaxTriesReached(pData->m_ReadOnly) && connector.ConnectSqlServer(pData->m_ReadOnly))
		{
			try {
				if (pData->m_pFuncPtr(connector.SqlServer(), pData->m_pSqlData, false))
					Success = true;
			} catch (...) {
				dbg_msg("sql", "Unexpected exception caught");
			}

			// disconnect from database server
			connector.SqlServer()->Disconnect();
		}

		// handle failures
		// eg write inserts to a file and print a nice error message
		if (!Success)
			pData->m_pFuncPtr(0, pData->m_pSqlData, true);
	} catch (...) {
		dbg_msg("sql", "Unexpected exception caught");
	}

	delete pData->m_pSqlData;
	delete pData;
}

CSqlScore::CSqlScore(CGameContext *pGameServer) :
		m_pGameServer(pGameServer),
		m_pServer(pGameServer->Server())
{
	CSqlConnector::ResetReachable();

	auto InitResult = std::make_shared<CSqlInitResult>();
	CSqlInitData *Tmp = new CSqlInitData(InitResult);
	((CGameControllerDDRace*)(pGameServer->m_pController))->m_pInitResult = InitResult;
	Tmp->m_Map = g_Config.m_SvMap;

	IOHANDLE File = GameServer()->Storage()->OpenFile("wordlist.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		dbg_msg("sql", "failed to open wordlist");
		Server()->SetErrorShutdown("sql open wordlist error");
		return;
	}

	uint64 aSeed[2];
	secure_random_fill(aSeed, sizeof(aSeed));
	m_Prng.Seed(aSeed);
	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		char Word[32] = {0};
		sscanf(pLine, "%*s %31s", Word);
		Word[31] = 0;
		m_aWordlist.push_back(Word);
	}
	if(m_aWordlist.size() < 1000)
	{
		dbg_msg("sql", "too few words in wordlist");
		Server()->SetErrorShutdown("sql too few words in wordlist");
		return;
	}
	thread_init_and_detach(CSqlExecData<CSqlInitResult>::ExecSqlFunc,
			new CSqlExecData<CSqlInitResult>(Init, Tmp),
			"SqlScore constructor");
}

bool CSqlScore::Init(CSqlServer* pSqlServer, const CSqlData<CSqlInitResult> *pGameData, bool HandleFailure)
{
	const CSqlInitData *pData = dynamic_cast<const CSqlInitData *>(pGameData);

	if (HandleFailure)
	{
		dbg_msg("sql", "FATAL ERROR: Could not init SqlScore");
		return true;
	}

	try
	{
		char aBuf[512];
		// get the best time
		str_format(aBuf, sizeof(aBuf),
				"SELECT Time FROM %s_race WHERE Map='%s' ORDER BY `Time` ASC LIMIT 1;",
				pSqlServer->GetPrefix(), pData->m_Map.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->next())
			pData->m_pResult->m_CurrentRecord = (float)pSqlServer->GetResults()->getDouble("Time");

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Getting best time on server done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
	}
	return false;
}

void CSqlScore::LoadPlayerData(int ClientID)
{
	ExecPlayerThread(LoadPlayerDataThread, "load player data", ClientID, "", 0);
}

// update stuff
bool CSqlScore::LoadPlayerDataThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	pData->m_pResult->SetVariant(CSqlPlayerResult::PLAYER_INFO);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		// get best race time
		str_format(aBuf, sizeof(aBuf),
				"SELECT * "
				"FROM %s_race "
				"WHERE Map='%s' AND Name='%s' "
				"ORDER BY Time ASC "
				"LIMIT 1;",
				pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_RequestingPlayer.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);
		if(pSqlServer->GetResults()->next())
		{
			// get the best time
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			pData->m_pResult->m_Data.m_Info.m_Time = Time;
			pData->m_pResult->m_Data.m_Info.m_Score = -Time;
			pData->m_pResult->m_Data.m_Info.m_HasFinishScore = true;

			char aColumn[8];
			if(g_Config.m_SvCheckpointSave)
			{
				for(int i = 0; i < NUM_CHECKPOINTS; i++)
				{
					str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
					pData->m_pResult->m_Data.m_Info.m_CpTime[i] = (float)pSqlServer->GetResults()->getDouble(aColumn);
				}
			}
		}

		// birthday check
		str_format(aBuf, sizeof(aBuf),
				"SELECT YEAR(Current) - YEAR(Stamp) AS YearsAgo "
				"FROM ("
					"SELECT CURRENT_TIMESTAMP AS Current, MIN(Timestamp) AS Stamp "
					"FROM %s_race "
					"WHERE Name='%s'"
				") AS l "
				"WHERE DAYOFMONTH(Current) = DAYOFMONTH(Stamp) AND MONTH(Current) = MONTH(Stamp) "
					"AND YEAR(Current) > YEAR(Stamp);",
				pSqlServer->GetPrefix(), pData->m_RequestingPlayer.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->next())
		{
			int YearsAgo = pSqlServer->GetResults()->getInt("YearsAgo");
			pData->m_pResult->m_Data.m_Info.m_Birthday = YearsAgo;
		}
		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Finished loading player data");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update account");
	}
	return false;
}

void CSqlScore::MapVote(int ClientID, const char* MapName)
{
	ExecPlayerThread(MapVoteThread, "map vote", ClientID, MapName, 0);
}

bool CSqlScore::MapVoteThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	if (HandleFailure)
		return true;

	try
	{
		char aFuzzyMap[128];
		str_copy(aFuzzyMap, pData->m_Name.Str(), sizeof(aFuzzyMap));
		sqlstr::ClearString(aFuzzyMap, sizeof(aFuzzyMap));
		sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

		char aBuf[768];
		str_format(aBuf, sizeof(aBuf),
				"SELECT Map, Server "
				"FROM %s_maps "
				"WHERE Map LIKE '%s' COLLATE utf8mb4_general_ci "
				"ORDER BY "
					"CASE WHEN Map = '%s' THEN 0 ELSE 1 END, "
					"CASE WHEN Map LIKE '%s%%' THEN 0 ELSE 1 END, "
					"LENGTH(Map), Map "
				"LIMIT 1;",
				pSqlServer->GetPrefix(), aFuzzyMap,
				pData->m_Name.ClrStr(), pData->m_Name.ClrStr()
		);
		pSqlServer->executeSqlQuery(aBuf);
		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(paMessages[0], sizeof(paMessages[0]),
					"No map like \"%s\" found. "
					"Try adding a '%%' at the start if you don't know the first character. "
					"Example: /map %%castle for \"Out of Castle\"",
					pData->m_Name.Str());
		}
		else
		{
			pSqlServer->GetResults()->first();
			auto Server = pSqlServer->GetResults()->getString("Server");
			auto Map = pSqlServer->GetResults()->getString("Map");
			pData->m_pResult->SetVariant(CSqlPlayerResult::MAP_VOTE);
			auto MapVote = &pData->m_pResult->m_Data.m_MapVote;
			strcpy(MapVote->m_Reason, "/map");
			str_copy(MapVote->m_Server, Server.c_str(), sizeof(MapVote->m_Server));
			str_copy(MapVote->m_Map, Map.c_str(), sizeof(MapVote->m_Map));

			for(char *p = MapVote->m_Server; *p; p++) // lower case server
				*p = tolower(*p);
		}
		pData->m_pResult->m_Done = true;
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not start Mapvote");
	}
	return false;
}

void CSqlScore::MapInfo(int ClientID, const char* MapName)
{
	ExecPlayerThread(MapInfoThread, "map info", ClientID, MapName, 0);
}

bool CSqlScore::MapInfoThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aFuzzyMap[128];
		str_copy(aFuzzyMap, pData->m_Name.Str(), sizeof(aFuzzyMap));
		sqlstr::ClearString(aFuzzyMap, sizeof(aFuzzyMap));
		sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf),
				"SELECT l.Map, l.Server, Mapper, Points, Stars, "
					"(select count(Name) from %s_race where Map = l.Map) as Finishes, "
					"(select count(distinct Name) from %s_race where Map = l.Map) as Finishers, "
					"(select round(avg(Time)) from %s_race where Map = l.Map) as Average, "
					"UNIX_TIMESTAMP(l.Timestamp) as Stamp, "
					"UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(l.Timestamp) as Ago, "
					"(select min(Time) from %s_race where Map = l.Map and Name = '%s') as OwnTime "
				"FROM ("
					"SELECT * FROM %s_maps "
					"WHERE Map LIKE '%s' COLLATE utf8mb4_general_ci "
					"ORDER BY "
						"CASE WHEN Map = '%s' THEN 0 ELSE 1 END, "
						"CASE WHEN Map LIKE '%s%%' THEN 0 ELSE 1 END, "
						"LENGTH(Map), "
						"Map "
					"LIMIT 1"
				") as l;",
				pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
				pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
				pData->m_RequestingPlayer.ClrStr(),
				pSqlServer->GetPrefix(),
				aFuzzyMap,
				pData->m_Name.ClrStr(),
				pData->m_Name.ClrStr()
		);
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
					"No map like \"%s\" found.", pData->m_Name.Str());
		}
		else
		{
			pSqlServer->GetResults()->next();
			int Points = pSqlServer->GetResults()->getInt("Points");
			int Stars = pSqlServer->GetResults()->getInt("Stars");
			int finishes = pSqlServer->GetResults()->getInt("Finishes");
			int finishers = pSqlServer->GetResults()->getInt("Finishers");
			int average = pSqlServer->GetResults()->getInt("Average");
			char aMap[128];
			strcpy(aMap, pSqlServer->GetResults()->getString("Map").c_str());
			char aServer[32];
			strcpy(aServer, pSqlServer->GetResults()->getString("Server").c_str());
			char aMapper[128];
			strcpy(aMapper, pSqlServer->GetResults()->getString("Mapper").c_str());
			int stamp = pSqlServer->GetResults()->getInt("Stamp");
			int ago = pSqlServer->GetResults()->getInt("Ago");
			float ownTime = (float)pSqlServer->GetResults()->getDouble("OwnTime");

			char aAgoString[40] = "\0";
			char aReleasedString[60] = "\0";
			if(stamp != 0)
			{
				sqlstr::AgoTimeToString(ago, aAgoString);
				str_format(aReleasedString, sizeof(aReleasedString), ", released %s ago", aAgoString);
			}

			char aAverageString[60] = "\0";
			if(average > 0)
			{
				str_format(aAverageString, sizeof(aAverageString), " in %d:%02d average", average / 60, average % 60);
			}

			char aStars[20];
			switch(Stars)
			{
				case 0: strcpy(aStars, "✰✰✰✰✰"); break;
				case 1: strcpy(aStars, "★✰✰✰✰"); break;
				case 2: strcpy(aStars, "★★✰✰✰"); break;
				case 3: strcpy(aStars, "★★★✰✰"); break;
				case 4: strcpy(aStars, "★★★★✰"); break;
				case 5: strcpy(aStars, "★★★★★"); break;
				default: aStars[0] = '\0';
			}

			char aOwnFinishesString[40] = "\0";
			if(ownTime > 0)
			{
				str_format(aOwnFinishesString, sizeof(aOwnFinishesString),
						", your time: %02d:%05.2f", (int)(ownTime/60), ownTime-((int)ownTime/60*60)
				);
			}

			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
					"\"%s\" by %s on %s, %s, %d %s%s, %d %s by %d %s%s%s",
					aMap, aMapper, aServer, aStars,
					Points, Points == 1 ? "point" : "points",
					aReleasedString,
					finishes, finishes == 1 ? "finish" : "finishes",
					finishers, finishers == 1 ? "tee" : "tees",
					aAverageString, aOwnFinishesString
			);
		}
		pData->m_pResult->m_Done = true;
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not get Mapinfo");
	}
	return false;
}

void CSqlScore::SaveScore(int ClientID, float Time, const char *pTimestamp, float CpTime[NUM_CHECKPOINTS], bool NotEligible)
{
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated || NotEligible)
		return;

	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientID];
	if(pCurPlayer->m_SqlFinishResult != nullptr)
		dbg_msg("sql", "WARNING: previous save score result didn't complete, overwriting it now");
	pCurPlayer->m_SqlFinishResult = std::make_shared<CSqlPlayerResult>();
	CSqlScoreData *Tmp = new CSqlScoreData(pCurPlayer->m_SqlFinishResult);
	Tmp->m_Map = g_Config.m_SvMap;
	FormatUuid(GameServer()->GameUuid(), Tmp->m_GameUuid, sizeof(Tmp->m_GameUuid));
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCpCurrent[i] = CpTime[i];

	thread_init_and_detach(CSqlExecData<CSqlPlayerResult>::ExecSqlFunc,
			new CSqlExecData<CSqlPlayerResult>(SaveScoreThread, Tmp),
			"save score");
}

bool CSqlScore::SaveScoreThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	if(HandleFailure)
	{
		if(!g_Config.m_SvSqlFailureFile[0])
			return true;

		lock_wait(ms_FailureFileLock);
		IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
		if(File == 0)
		{
			lock_unlock(ms_FailureFileLock);
			dbg_msg("sql", "ERROR: Could not save Score, NOT even to a file");
			return false;
		}
		dbg_msg("sql", "ERROR: Could not save Score, writing insert to a file now...");

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf),
				"INSERT IGNORE INTO %%s_race(Map, Name, Timestamp, Time, Server, "
					"cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, "
					"cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, "
					"GameID, DDNet7) "
				"VALUES ('%s', '%s', '%s', '%.2f', '%s',"
					"'%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', "
					"'%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', "
					"'%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', "
					"'%.2f', '%s', false);",
				pData->m_Map.ClrStr(), pData->m_Name.ClrStr(),
				pData->m_aTimestamp, pData->m_Time, g_Config.m_SvSqlServerName,
				pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2],
				pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5],
				pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8],
				pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11],
				pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14],
				pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17],
				pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20],
				pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23],
				pData->m_aCpCurrent[24],
				pData->m_GameUuid);
		io_write(File, aBuf, str_length(aBuf));
		io_write_newline(File);
		io_close(File);
		lock_unlock(ms_FailureFileLock);

		pData->m_pResult->SetVariant(CSqlPlayerResult::BROADCAST);
		strcpy(pData->m_pResult->m_Data.m_Broadcast,
				"Database connection failed, score written to a file instead. Admins will add it manually in a few days.");
		pData->m_pResult->m_Done = true;
		return true;
	}

	try
	{
		char aBuf[1024];

		str_format(aBuf, sizeof(aBuf),
				"SELECT COUNT(*) AS NumFinished FROM %s_race WHERE Map='%s' AND Name='%s' ORDER BY time ASC LIMIT 1;",
				pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);
		pSqlServer->GetResults()->first();
		int NumFinished = pSqlServer->GetResults()->getInt("NumFinished");
		if(NumFinished == 0)
		{
			str_format(aBuf, sizeof(aBuf), "SELECT Points FROM %s_maps WHERE Map='%s'", pSqlServer->GetPrefix(), pData->m_Map.ClrStr());
			pSqlServer->executeSqlQuery(aBuf);

			if(pSqlServer->GetResults()->rowsCount() == 1)
			{
				pSqlServer->GetResults()->next();
				int Points = pSqlServer->GetResults()->getInt("Points");
				str_format(paMessages[0], sizeof(paMessages[0]), "You earned %d point%s for finishing this map!", Points, Points == 1 ? "" : "s");

				str_format(aBuf, sizeof(aBuf),
						"INSERT INTO %s_points(Name, Points) "
						"VALUES ('%s', '%d') "
						"ON duplicate key "
							"UPDATE Name=VALUES(Name), Points=Points+VALUES(Points);",
						pSqlServer->GetPrefix(), pData->m_Name.ClrStr(), Points);
				pSqlServer->executeSql(aBuf);
			}
		}

		// save score
		str_format(aBuf, sizeof(aBuf),
				"INSERT IGNORE INTO %s_race("
					"Map, Name, Timestamp, Time, Server, "
					"cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, "
					"cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, "
					"GameID, DDNet7) "
				"VALUES ('%s', '%s', '%s', '%.2f', '%s', "
					"'%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', "
					"'%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', "
					"'%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', "
					"'%s', false);",
				pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(),
				pData->m_aTimestamp, pData->m_Time, g_Config.m_SvSqlServerName,
				pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2],
				pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5],
				pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8],
				pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11],
				pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14],
				pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17],
				pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20],
				pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23],
				pData->m_aCpCurrent[24], pData->m_GameUuid);
		dbg_msg("sql", "%s", aBuf);
		pSqlServer->executeSql(aBuf);

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Saving score done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not insert time");
	}
	return false;
}

void CSqlScore::SaveTeamScore(int* aClientIDs, unsigned int Size, float Time, const char *pTimestamp)
{
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	for(unsigned int i = 0; i < Size; i++)
	{
		if(GameServer()->m_apPlayers[aClientIDs[i]]->m_NotEligibleForFinish)
			return;
	}
	CSqlTeamScoreData *Tmp = new CSqlTeamScoreData(nullptr);
	for(unsigned int i = 0; i < Size; i++)
		Tmp->m_aNames[i] = Server()->ClientName(aClientIDs[i]);
	Tmp->m_Size = Size;
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_GameUuid, sizeof(Tmp->m_GameUuid));
	Tmp->m_Map = g_Config.m_SvMap;

	thread_init_and_detach(CSqlExecData<void>::ExecSqlFunc,
			new CSqlExecData<void>(SaveTeamScoreThread, Tmp),
			"save team score");
}

bool CSqlScore::SaveTeamScoreThread(CSqlServer* pSqlServer, const CSqlData<void> *pGameData, bool HandleFailure)
{
	const CSqlTeamScoreData *pData = dynamic_cast<const CSqlTeamScoreData *>(pGameData);

	if(HandleFailure)
	{
		if(!g_Config.m_SvSqlFailureFile[0])
			return true;

		dbg_msg("sql", "ERROR: Could not save TeamScore, writing insert to a file now...");

		lock_wait(ms_FailureFileLock);
		IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
		if(File)
		{
			const char aUUID[] = "SET @id = UUID();";
			io_write(File, aUUID, sizeof(aUUID) - 1);
			io_write_newline(File);

			char aBuf[2300];
			for(unsigned int i = 0; i < pData->m_Size; i++)
			{
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %%s_teamrace(Map, Name, Timestamp, Time, ID, GameID, DDNet7) VALUES ('%s', '%s', '%s', '%.2f', @id, '%s', false);", pData->m_Map.ClrStr(), pData->m_aNames[i].ClrStr(), pData->m_aTimestamp, pData->m_Time, pData->m_GameUuid);
				io_write(File, aBuf, str_length(aBuf));
				io_write_newline(File);
			}
			io_close(File);
			lock_unlock(ms_FailureFileLock);
			return true;
		}
		lock_unlock(ms_FailureFileLock);
		return false;
	}

	try
	{
		char aBuf[2300];

		// get the names sorted in a tab separated string
		const sqlstr::CSqlString<MAX_NAME_LENGTH> *apNames[MAX_CLIENTS];
		for(unsigned int i = 0; i < pData->m_Size; i++)
			apNames[i] = &pData->m_aNames[i];
		std::sort(apNames, apNames+pData->m_Size);
		char aSortedNames[2048] = {0};
		for(unsigned int i = 0; i < pData->m_Size; i++)
		{
			if(i != 0)
				str_append(aSortedNames, "\t", sizeof(aSortedNames));
			str_append(aSortedNames, apNames[i]->ClrStr(), sizeof(aSortedNames));
		}
		str_format(aBuf, sizeof(aBuf),
				"SELECT l.ID, Time "
				"FROM ((" // preselect teams with first name in team
						"SELECT ID "
						"FROM %s_teamrace "
						"WHERE Map = '%s' AND Name = '%s' AND DDNet7 = false"
					") as l"
				") INNER JOIN %s_teamrace AS r ON l.ID = r.ID "
				"GROUP BY ID "
				"HAVING GROUP_CONCAT(Name ORDER BY Name SEPARATOR '\t') = '%s'",
				pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_aNames[0].ClrStr(),
				pSqlServer->GetPrefix(), aSortedNames);
		pSqlServer->executeSqlQuery(aBuf);

		if (pSqlServer->GetResults()->rowsCount() > 0)
		{
			pSqlServer->GetResults()->first();
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			auto ID = pSqlServer->GetResults()->getString("ID");
			dbg_msg("sql", "found team rank from same team (old time: %f, new time: %f)", Time, pData->m_Time);
			if(pData->m_Time < Time)
			{
				str_format(aBuf, sizeof(aBuf),
						"UPDATE %s_teamrace SET Time='%.2f', Timestamp='%s', DDNet7=false WHERE ID = '%s';",
						pSqlServer->GetPrefix(), pData->m_Time, pData->m_aTimestamp, ID.c_str());
				dbg_msg("sql", "%s", aBuf);
				pSqlServer->executeSql(aBuf);
			}
		}
		else
		{
			pSqlServer->executeSql("SET @id = UUID();");

			for(unsigned int i = 0; i < pData->m_Size; i++)
			{
				// if no entry found... create a new one
				str_format(aBuf, sizeof(aBuf),
						"INSERT IGNORE INTO %s_teamrace(Map, Name, Timestamp, Time, ID, GameID, DDNet7) "
						"VALUES ('%s', '%s', '%s', '%.2f', @id, '%s', false);",
						pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_aNames[i].ClrStr(),
						pData->m_aTimestamp, pData->m_Time, pData->m_GameUuid);
				dbg_msg("sql", "%s", aBuf);
				pSqlServer->executeSql(aBuf);
			}
		}

		dbg_msg("sql", "Updating team time done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update time");
	}
	return false;
}

void CSqlScore::ShowRank(int ClientID, const char* pName)
{
	ExecPlayerThread(ShowRankThread, "show rank", ClientID, pName, 0);
}

bool CSqlScore::ShowRankThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	if (HandleFailure)
	{
		pData->m_pResult->m_Done = true;
		return true;
	}

	try
	{
		// check sort method
		char aBuf[600];

		str_format(aBuf, sizeof(aBuf),
				"SELECT Rank, Name, Time "
				"FROM ("
					"SELECT RANK() OVER w AS Rank, Name, MIN(Time) AS Time "
					"FROM %s_race "
					"WHERE Map = '%s' "
					"GROUP BY Name "
					"WINDOW w AS (ORDER BY Time)"
				") as a "
				"WHERE Name = '%s';",
				pSqlServer->GetPrefix(),
				pData->m_Map.ClrStr(),
				pData->m_Name.ClrStr()
		);

		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
					"%s is not ranked", pData->m_Name.Str());
		}
		else
		{
			pSqlServer->GetResults()->next();

			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = pSqlServer->GetResults()->getInt("Rank");
			if(g_Config.m_SvHideScore)
			{
				str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
						"Your time: %02d:%05.2f", (int)(Time/60), Time-((int)Time/60*60));
			}
			else
			{
				pData->m_pResult->m_MessageKind = CSqlPlayerResult::ALL;
				str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
						"%d. %s Time: %02d:%05.2f, requested by %s",
						Rank, pSqlServer->GetResults()->getString("Name").c_str(),
						(int)(Time/60), Time-((int)Time/60*60), pData->m_RequestingPlayer.Str());
			}
		}

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing rank done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show rank");
	}
	return false;
}

void CSqlScore::ShowTeamRank(int ClientID, const char* pName)
{
	ExecPlayerThread(ShowTeamRankThread, "show team rank", ClientID, pName, 0);
}

bool CSqlScore::ShowTeamRankThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	if (HandleFailure)
	{
		pData->m_pResult->m_Done = true;
		return true;
	}

	try
	{
		// check sort method
		char aBuf[2400];
		char aNames[2300];
		aNames[0] = '\0';

		str_format(aBuf, sizeof(aBuf),
				"SELECT Time, Rank, Name "
				"FROM (" // teamrank score board
					"SELECT RANK() OVER w AS Rank, Id "
					"FROM %s_teamrace "
					"WHERE Map = '%s' "
					"GROUP BY Id "
					"WINDOW w AS (ORDER BY Time)"
				") as l "
				"INNER JOIN %s_teamrace as r ON l.ID = r.ID "
				"WHERE l.ID = (" // find id for top teamrank of player
					"SELECT Id "
					"FROM %s_teamrace "
					"WHERE Map = '%s' AND Name = '%s' "
					"ORDER BY Time "
					"LIMIT 1"
				") "
				"ORDER BY Name;",
				pSqlServer->GetPrefix(),
				pData->m_Map.ClrStr(),
				pSqlServer->GetPrefix(),
				pSqlServer->GetPrefix(),
				pData->m_Map.ClrStr(),
				pData->m_Name.ClrStr()
		);

		pSqlServer->executeSqlQuery(aBuf);

		int Rows = pSqlServer->GetResults()->rowsCount();

		if(Rows < 1)
		{
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
					"%s has no team ranks", pData->m_Name.Str());
		}
		else
		{
			pSqlServer->GetResults()->first();
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = pSqlServer->GetResults()->getInt("Rank");

			for(int Row = 0; Row < Rows; Row++)
			{
				str_append(aNames, pSqlServer->GetResults()->getString("Name").c_str(), sizeof(aNames));
				pSqlServer->GetResults()->next();

				if (Row < Rows - 2)
					str_append(aNames, ", ", sizeof(aNames));
				else if (Row < Rows - 1)
					str_append(aNames, " & ", sizeof(aNames));
			}

			if(g_Config.m_SvHideScore)
			{
				str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
						"Your team time: %02d:%05.02f", (int)(Time/60), Time-((int)Time/60*60));
			}
			else
			{
				pData->m_pResult->m_MessageKind = CSqlPlayerResult::ALL;
				str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
						"%d. %s Team time: %02d:%05.02f, requested by %s",
						Rank, aNames, (int)(Time/60), Time-((int)Time/60*60), pData->m_RequestingPlayer.Str());
			}
		}

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing teamrank done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show team rank");
	}
	return false;
}

void CSqlScore::ShowTop5(int ClientID, int Offset)
{
	ExecPlayerThread(ShowTop5Thread, "show top5", ClientID, "", Offset);
}

bool CSqlScore::ShowTop5Thread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	if (HandleFailure)
		return true;

	int LimitStart = maximum(abs(pData->m_Offset)-1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	try
	{
		// check sort method
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
				"SELECT Name, Time, Rank "
				"FROM ("
					"SELECT RANK() OVER w AS Rank, Name, MIN(Time) AS Time "
					"FROM %s_race "
					"WHERE Map = '%s' "
					"GROUP BY Name "
					"WINDOW w AS (ORDER BY Time)"
				") as a "
				"ORDER BY Rank %s "
				"LIMIT %d, 5;",
				pSqlServer->GetPrefix(),
				pData->m_Map.ClrStr(),
				pOrder,
				LimitStart
		);
		pSqlServer->executeSqlQuery(aBuf);

		// show top5
		strcpy(pData->m_pResult->m_Data.m_aaMessages[0], "----------- Top 5 -----------");

		int Line = 1;
		while(pSqlServer->GetResults()->next())
		{
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = pSqlServer->GetResults()->getInt("Rank");
			str_format(pData->m_pResult->m_Data.m_aaMessages[Line], sizeof(pData->m_pResult->m_Data.m_aaMessages[Line]),
					"%d. %s Time: %02d:%05.2f",
					Rank, pSqlServer->GetResults()->getString("Name").c_str(),
					(int)(Time/60), Time-((int)Time/60*60)
			);
			Line++;
		}
		strcpy(pData->m_pResult->m_Data.m_aaMessages[Line], "-------------------------------");

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing top5 done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show top5");
	}

	return false;
}

void CSqlScore::ShowTeamTop5(int ClientID, int Offset)
{
	ExecPlayerThread(ShowTeamTop5Thread, "show team top5", ClientID, "", Offset);
}

bool CSqlScore::ShowTeamTop5Thread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;
	if (HandleFailure)
		return true;

	int LimitStart = maximum(abs(pData->m_Offset)-1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	try
	{
		// check sort method
		char aBuf[512];

		str_format(aBuf, sizeof(aBuf),
				"SELECT Name, Time, Rank, TeamSize "
				"FROM (" // limit to 5
					"SELECT Rank, ID, TeamSize "
					"FROM (" // teamrank score board
						"SELECT RANK() OVER w AS Rank, ID, COUNT(*) AS Teamsize "
						"FROM %s_teamrace "
						"WHERE Map = '%s' "
						"GROUP BY Id "
						"WINDOW w AS (ORDER BY Time)"
					") as l1 "
					"ORDER BY Rank %s "
					"LIMIT %d, 5"
				") as l2 "
				"INNER JOIN %s_teamrace as r ON l2.ID = r.ID "
				"ORDER BY Rank %s, r.ID, Name ASC;",
				pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pOrder, LimitStart, pSqlServer->GetPrefix(), pOrder
		);
		pSqlServer->executeSqlQuery(aBuf);

		// show teamtop5
		pSqlServer->GetResults()->first();
		strcpy(paMessages[0], "------- Team Top 5 -------");
		int Line;
		for(Line = 1; Line < 6; Line++) // print
		{
			if(pSqlServer->GetResults()->isAfterLast())
				break;
			int TeamSize = pSqlServer->GetResults()->getInt("TeamSize");
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = pSqlServer->GetResults()->getInt("Rank");

			char aNames[2300] = { 0 };
			for(int i = 0; i < TeamSize; i++)
			{
				auto Name = pSqlServer->GetResults()->getString("Name");
				str_append(aNames, Name.c_str(), sizeof(aNames));
				if (i < TeamSize - 2)
					str_append(aNames, ", ", sizeof(aNames));
				else if (i == TeamSize - 2)
					str_append(aNames, " & ", sizeof(aNames));
				pSqlServer->GetResults()->next();
			}
			str_format(paMessages[Line], sizeof(paMessages[Line]), "%d. %s Team Time: %02d:%05.2f",
					Rank, aNames, (int)(Time/60), Time-((int)Time/60*60));
		}
		strcpy(paMessages[Line], "-------------------------------");

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing teamtop5 done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show teamtop5");
	}
	return false;
}

void CSqlScore::ShowTimes(int ClientID, int Offset)
{
	ExecPlayerThread(ShowTimesThread, "show times", ClientID, "", Offset);
}

void CSqlScore::ShowTimes(int ClientID, const char* pName, int Offset)
{
	ExecPlayerThread(ShowTimesThread, "show times", ClientID, pName, Offset);
}

bool CSqlScore::ShowTimesThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	if (HandleFailure)
		return true;

	int LimitStart = maximum(abs(pData->m_Offset)-1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "DESC" : "ASC";

	try
	{
		char aBuf[512];

		if(pData->m_Name.Str()[0] != '\0') // last 5 times of a player
		{
			str_format(aBuf, sizeof(aBuf),
					"SELECT Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp "
					"FROM %s_race "
					"WHERE Map = '%s' AND Name = '%s' "
					"ORDER BY Timestamp %s "
					"LIMIT %d, 5;",
					pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pOrder, LimitStart
			);
		}
		else // last 5 times of server
		{
			str_format(aBuf, sizeof(aBuf),
					"SELECT Name, Time, "
						"UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, "
						"UNIX_TIMESTAMP(Timestamp) as Stamp "
					"FROM %s_race "
					"WHERE Map = '%s' "
					"ORDER BY Timestamp %s "
					"LIMIT %d, 5;",
					pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pOrder, LimitStart
			);
		}
		pSqlServer->executeSqlQuery(aBuf);

		// show top5
		if(pSqlServer->GetResults()->rowsCount() == 0)
		{
			strcpy(paMessages[0], "There are no times in the specified range");
			pData->m_pResult->m_Done = true;
			return true;
		}

		strcpy(paMessages[0], "------------- Last Times -------------");
		int Line = 1;
		while(pSqlServer->GetResults()->next())
		{
			char aAgoString[40] = "\0";
			int pSince = pSqlServer->GetResults()->getInt("Ago");
			int pStamp = pSqlServer->GetResults()->getInt("Stamp");
			float pTime = (float)pSqlServer->GetResults()->getDouble("Time");

			sqlstr::AgoTimeToString(pSince, aAgoString);

			if(pData->m_Name.Str()[0] != '\0') // last 5 times of a player
			{
				if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
					str_format(paMessages[Line], sizeof(paMessages[Line]),
							"%02d:%05.02f, don't know how long ago",
							(int)(pTime/60), pTime-((int)pTime/60*60));
				else
					str_format(paMessages[Line], sizeof(paMessages[Line]),
							"%s ago, %02d:%05.02f",
							aAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
			}
			else // last 5 times of the server
			{
				auto Name = pSqlServer->GetResults()->getString("Name");
				if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
					str_format(paMessages[Line], sizeof(paMessages[Line]),
							"%s, %02d:%05.02f, don't know when",
							Name.c_str(), (int)(pTime/60), pTime-((int)pTime/60*60));
				else
					str_format(paMessages[Line], sizeof(paMessages[Line]),
							"%s, %s ago, %02d:%05.02f",
							Name.c_str(), aAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
			}
			Line++;
		}
		strcpy(paMessages[Line], "----------------------------------------------------");

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing times done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show times");
	}
	return false;
}

void CSqlScore::ShowPoints(int ClientID, const char* pName)
{
	ExecPlayerThread(ShowPointsThread, "show points", ClientID, pName, 0);
}

bool CSqlScore::ShowPointsThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
				"SELECT Rank, Points, Name "
				"FROM ("
					"SELECT RANK() OVER w AS Rank, Points, Name "
					"FROM %s_points "
					"WINDOW w as (ORDER BY Points DESC)"
				") as a "
				"WHERE Name = '%s';",
				pSqlServer->GetPrefix(), pData->m_Name.ClrStr()
		);
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(paMessages[0], sizeof(paMessages[0]),
					"%s has not collected any points so far", pData->m_Name.Str());
		}
		else
		{
			pSqlServer->GetResults()->next();
			int Count = pSqlServer->GetResults()->getInt("Points");
			int Rank = pSqlServer->GetResults()->getInt("Rank");
			auto Name = pSqlServer->GetResults()->getString("Name");
			pData->m_pResult->m_MessageKind = CSqlPlayerResult::ALL;
			str_format(paMessages[0], sizeof(paMessages[0]),
					"%d. %s Points: %d, requested by %s",
					Rank, Name.c_str(), Count, pData->m_RequestingPlayer.Str());
		}

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing points done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show points");
	}
	return false;
}

void CSqlScore::ShowTopPoints(int ClientID, int Offset)
{
	ExecPlayerThread(ShowTopPointsThread, "show top points", ClientID, "", Offset);
}

bool CSqlScore::ShowTopPointsThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	if (HandleFailure)
		return true;

	int LimitStart = maximum(abs(pData->m_Offset)-1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	try
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
				"SELECT Rank, Points, Name "
				"FROM ("
					"SELECT RANK() OVER w AS Rank, Points, Name "
					"FROM %s_points "
					"WINDOW w as (ORDER BY Points DESC)"
				") as a "
				"ORDER BY Rank %s "
				"LIMIT %d, 5;",
				pSqlServer->GetPrefix(), pOrder, LimitStart
		);

		pSqlServer->executeSqlQuery(aBuf);

		// show top points
		strcpy(paMessages[0], "-------- Top Points --------");

		int Line = 1;
		while(pSqlServer->GetResults()->next())
		{
			int Rank = pSqlServer->GetResults()->getInt("Rank");
			auto Name = pSqlServer->GetResults()->getString("Name");
			int Points = pSqlServer->GetResults()->getInt("Points");
			str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%d. %s Points: %d", Rank, Name.c_str(), Points);
			Line++;
		}
		strcpy(paMessages[Line], "-------------------------------");

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing toppoints done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show toppoints");
	}
	return false;
}

void CSqlScore::RandomMap(int ClientID, int Stars)
{
	auto pResult = std::make_shared<CSqlRandomMapResult>(ClientID);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto *Tmp = new CSqlRandomMapRequest(pResult);
	Tmp->m_Stars = Stars;
	Tmp->m_CurrentMap = g_Config.m_SvMap;
	Tmp->m_ServerType = g_Config.m_SvServerType;
	Tmp->m_RequestingPlayer = GameServer()->Server()->ClientName(ClientID);

	thread_init_and_detach(
			CSqlExecData<CSqlRandomMapResult>::ExecSqlFunc,
			new CSqlExecData<CSqlRandomMapResult>(RandomMapThread, Tmp),
			"random map");
}

bool CSqlScore::RandomMapThread(CSqlServer* pSqlServer, const CSqlData<CSqlRandomMapResult> *pGameData, bool HandleFailure)
{
	const CSqlRandomMapRequest *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		if(0 <= pData->m_Stars && pData->m_Stars <= 5)
		{
			str_format(aBuf, sizeof(aBuf),
					"SELECT * FROM %s_maps "
					"WHERE Server = \"%s\" AND Map != \"%s\" AND Stars = \"%d\" "
					"ORDER BY RAND() LIMIT 1;",
					pSqlServer->GetPrefix(),
					pData->m_ServerType.ClrStr(),
					pData->m_CurrentMap.ClrStr(),
					pData->m_Stars
			);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf),
					"SELECT * FROM %s_maps "
					"WHERE Server = \"%s\" AND Map != \"%s\" "
					"ORDER BY RAND() LIMIT 1;",
					pSqlServer->GetPrefix(),
					pData->m_ServerType.ClrStr(),
					pData->m_CurrentMap.ClrStr()
			);
		}
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_copy(pData->m_pResult->m_aMessage, "No maps found on this server!", sizeof(pData->m_pResult->m_aMessage));
		}
		else
		{
			pSqlServer->GetResults()->next();
			auto Map = pSqlServer->GetResults()->getString("Map");
			str_copy(pData->m_pResult->m_Map, Map.c_str(), sizeof(pData->m_pResult->m_Map));
		}

		dbg_msg("sql", "voting random map done");
		pData->m_pResult->m_Done = true;
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not vote random map");
	}
	return false;
}

void CSqlScore::RandomUnfinishedMap(int ClientID, int Stars)
{
	auto pResult = std::make_shared<CSqlRandomMapResult>(ClientID);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto *Tmp = new CSqlRandomMapRequest(pResult);
	Tmp->m_Stars = Stars;
	Tmp->m_CurrentMap = g_Config.m_SvMap;
	Tmp->m_ServerType = g_Config.m_SvServerType;
	Tmp->m_RequestingPlayer = GameServer()->Server()->ClientName(ClientID);

	thread_init_and_detach(
			CSqlExecData<CSqlRandomMapResult>::ExecSqlFunc,
			new CSqlExecData<CSqlRandomMapResult>(RandomUnfinishedMapThread, Tmp),
			"random unfinished map");
}

bool CSqlScore::RandomUnfinishedMapThread(CSqlServer* pSqlServer, const CSqlData<CSqlRandomMapResult> *pGameData, bool HandleFailure)
{
	const CSqlRandomMapRequest *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		if(pData->m_Stars >= 0)
		{
			str_format(aBuf, sizeof(aBuf),
					"SELECT Map "
					"FROM %s_maps "
					"WHERE Server = \"%s\" AND Map != \"%s\" AND Stars = \"%d\" AND Map NOT IN ("
						"SELECT Map "
						"FROM %s_race "
						"WHERE Name = \"%s\""
					") ORDER BY RAND() "
					"LIMIT 1;",
					pSqlServer->GetPrefix(), pData->m_ServerType.ClrStr(), pData->m_CurrentMap.ClrStr(),
					pData->m_Stars, pSqlServer->GetPrefix(), pData->m_RequestingPlayer.ClrStr());
		}
		else
		{
			str_format(aBuf, sizeof(aBuf),
					"SELECT Map "
					"FROM %s_maps AS maps "
					"WHERE Server = \"%s\" AND Map != \"%s\" AND Map NOT IN ("
						"SELECT Map "
						"FROM %s_race as race "
						"WHERE Name = \"%s\""
					") ORDER BY RAND() "
					"LIMIT 1;",
					pSqlServer->GetPrefix(), pData->m_ServerType.ClrStr(), pData->m_CurrentMap.ClrStr(),
					pSqlServer->GetPrefix(), pData->m_RequestingPlayer.ClrStr());
		}
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_copy(pData->m_pResult->m_aMessage, "You have no more unfinished maps on this server!", sizeof(pData->m_pResult->m_aMessage));
		}
		else
		{
			pSqlServer->GetResults()->next();
			auto Map = pSqlServer->GetResults()->getString("Map");
			str_copy(pData->m_pResult->m_Map, Map.c_str(), sizeof(pData->m_pResult->m_Map));
		}

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "voting random unfinished map done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not vote random unfinished map");
	}
	return false;
}

void CSqlScore::SaveTeam(int ClientID, const char* Code, const char* Server)
{
	auto pController = ((CGameControllerDDRace*)(GameServer()->m_pController));
	int Team = pController->m_Teams.m_Core.Team(ClientID);
	if(pController->m_Teams.GetSaving(Team))
		return;

	auto SaveResult = std::make_shared<CSqlSaveResult>(ClientID, pController);
	int Result = SaveResult->m_SavedTeam.save(Team);
	if(CSaveTeam::HandleSaveError(Result, ClientID, GameServer()))
		return;
	pController->m_Teams.SetSaving(Team, SaveResult);

	CSqlTeamSave *Tmp = new CSqlTeamSave(SaveResult);
	str_copy(Tmp->m_Code, Code, sizeof(Tmp->m_Code));
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));
	Tmp->m_pResult->m_SaveID = RandomUuid();
	str_copy(Tmp->m_Server, Server, sizeof(Tmp->m_Server));
	str_copy(Tmp->m_ClientName, this->Server()->ClientName(ClientID), sizeof(Tmp->m_ClientName));
	Tmp->m_aGeneratedCode[0] = '\0';
	GeneratePassphrase(Tmp->m_aGeneratedCode, sizeof(Tmp->m_aGeneratedCode));

	pController->m_Teams.KillSavedTeam(ClientID, Team);
	thread_init_and_detach(
			CSqlExecData<CSqlSaveResult>::ExecSqlFunc,
			new CSqlExecData<CSqlSaveResult>(SaveTeamThread, Tmp, false),
			"save team");
}

bool CSqlScore::SaveTeamThread(CSqlServer* pSqlServer, const CSqlData<CSqlSaveResult> *pGameData, bool HandleFailure)
{
	const CSqlTeamSave *pData = dynamic_cast<const CSqlTeamSave *>(pGameData);

	char aSaveID[UUID_MAXSTRSIZE];
	FormatUuid(pData->m_pResult->m_SaveID, aSaveID, UUID_MAXSTRSIZE);

	char *pSaveState = pData->m_pResult->m_SavedTeam.GetString();
	if(HandleFailure)
	{
		if (!g_Config.m_SvSqlFailureFile[0])
			return true;

		lock_wait(ms_FailureFileLock);
		IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
		if(File)
		{
			dbg_msg("sql", "ERROR: Could not save Teamsave, writing insert to a file now...");
			sqlstr::CSqlString<65536> SaveState = pSaveState;
			sqlstr::CSqlString<128> Code = pData->m_aGeneratedCode;
			sqlstr::CSqlString<128> Map = pData->m_Map;

			char aBuf[65536];
			str_format(aBuf, sizeof(aBuf),
					"INSERT IGNORE INTO %%s_saves(Savegame, Map, Code, Timestamp, Server, SaveID, DDNet7) "
					"VALUES ('%s', '%s', '%s', CURRENT_TIMESTAMP(), '%s', '%s', false)",
					SaveState.ClrStr(), Map.ClrStr(),
					Code.ClrStr(), pData->m_Server, aSaveID
			);
			io_write(File, aBuf, str_length(aBuf));
			io_write_newline(File);
			io_close(File);
			lock_unlock(ms_FailureFileLock);

			pData->m_pResult->m_Status = CSqlSaveResult::SAVE_SUCCESS;
			strcpy(pData->m_pResult->m_aBroadcast,
					"Database connection failed, teamsave written to a file instead. Admins will add it manually in a few days.");
			str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
					"Team successfully saved by %s. Use '/load %s' to continue",
					pData->m_ClientName, Code.Str());
			return true;
		}
		lock_unlock(ms_FailureFileLock);
		dbg_msg("sql", "ERROR: Could not save Teamsave, NOT even to a file");
		return false;
	}

	try
	{
		char aBuf[65536];
		str_format(aBuf, sizeof(aBuf), "lock tables %s_saves write;", pSqlServer->GetPrefix());
		pSqlServer->executeSql(aBuf);

		char Code[128] = {0};
		str_format(aBuf, sizeof(aBuf), "SELECT Savegame FROM %s_saves WHERE Code = ? AND Map = ?", pSqlServer->GetPrefix());
		std::unique_ptr<sql::PreparedStatement> pPrepStmt;
		std::unique_ptr<sql::ResultSet> pResult;
		pPrepStmt.reset(pSqlServer->Connection()->prepareStatement(aBuf));
		bool UseCode = false;
		if(pData->m_Code[0] != '\0')
		{
			pPrepStmt->setString(1, pData->m_Code);
			pPrepStmt->setString(2, pData->m_Map);
			pResult.reset(pPrepStmt->executeQuery());
			if(pResult->rowsCount() == 0)
			{
				UseCode = true;
				str_copy(Code, pData->m_Code, sizeof(Code));
			}
		}
		if(!UseCode)
		{
			// use random generated passphrase if save code exists or no save code given
			pPrepStmt->setString(1, pData->m_aGeneratedCode);
			pPrepStmt->setString(2, pData->m_Map);
			pResult.reset(pPrepStmt->executeQuery());
			if(pResult->rowsCount() == 0)
			{
				UseCode = true;
				str_copy(Code, pData->m_aGeneratedCode, sizeof(Code));
			}
		}

		if(UseCode)
		{
			str_format(aBuf, sizeof(aBuf),
					"INSERT IGNORE INTO %s_saves(Savegame, Map, Code, Timestamp, Server, SaveID, DDNet7) "
					"VALUES (?, ?, ?, CURRENT_TIMESTAMP(), ?, ?, false)",
					pSqlServer->GetPrefix());
			pPrepStmt.reset(pSqlServer->Connection()->prepareStatement(aBuf));
			pPrepStmt->setString(1, pSaveState);
			pPrepStmt->setString(2, pData->m_Map);
			pPrepStmt->setString(3, Code);
			pPrepStmt->setString(4, pData->m_Server);
			pPrepStmt->setString(5, aSaveID);
			dbg_msg("sql", "%s", aBuf);
			pPrepStmt->execute();

			if(str_comp(pData->m_Server, g_Config.m_SvSqlServerName) == 0)
			{
				str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
						"Team successfully saved by %s. Use '/load %s' to continue",
						pData->m_ClientName, Code);
			}
			else
			{
				str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
						"Team successfully saved by %s. Use '/load %s' on %s to continue",
						pData->m_ClientName, Code, pData->m_Server);
			}
			pData->m_pResult->m_Status = CSqlSaveResult::SAVE_SUCCESS;
		}
		else
		{
			dbg_msg("sql", "ERROR: This save-code already exists");
			pData->m_pResult->m_Status = CSqlSaveResult::SAVE_FAILED;
			strcpy(pData->m_pResult->m_aMessage, "This save-code already exists");
		}
	}
	catch (sql::SQLException &e)
	{
		pData->m_pResult->m_Status = CSqlSaveResult::SAVE_FAILED;
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not save the team");

		strcpy(pData->m_pResult->m_aMessage, "MySQL Error: Could not save the team");
		pSqlServer->executeSql("unlock tables;");
		return false;
	}

	pSqlServer->executeSql("unlock tables;");
	return true;
}

void CSqlScore::LoadTeam(const char* Code, int ClientID)
{
	auto pController = ((CGameControllerDDRace*)(GameServer()->m_pController));
	int Team = pController->m_Teams.m_Core.Team(ClientID);
	if(pController->m_Teams.GetSaving(Team))
		return;
	if(Team <= 0 || Team >= MAX_CLIENTS)
	{
		GameServer()->SendChatTarget(ClientID, "You have to be in a team (from 1-63)");
		return;
	}
	if(pController->m_Teams.GetTeamState(Team) != CGameTeams::TEAMSTATE_OPEN)
	{
		GameServer()->SendChatTarget(ClientID, "Team can't be loaded while racing");
		return;
	}
	auto SaveResult = std::make_shared<CSqlSaveResult>(ClientID, pController);
	pController->m_Teams.SetSaving(Team, SaveResult);
	CSqlTeamLoad *Tmp = new CSqlTeamLoad(SaveResult);
	Tmp->m_Code = Code;
	Tmp->m_Map = g_Config.m_SvMap;
	Tmp->m_ClientID = ClientID;
	Tmp->m_RequestingPlayer = Server()->ClientName(ClientID);
	Tmp->m_NumPlayer = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pController->m_Teams.m_Core.Team(i) == Team)
		{
			// put all names at the beginning of the array
			str_copy(Tmp->m_aClientNames[Tmp->m_NumPlayer], Server()->ClientName(i), sizeof(Tmp->m_aClientNames[Tmp->m_NumPlayer]));
			Tmp->m_aClientID[Tmp->m_NumPlayer] = i;
			Tmp->m_NumPlayer++;
		}
	}
	thread_init_and_detach(
			CSqlExecData<CSqlSaveResult>::ExecSqlFunc,
			new CSqlExecData<CSqlSaveResult>(LoadTeamThread, Tmp, false),
			"load team");
}

bool CSqlScore::LoadTeamThread(CSqlServer* pSqlServer, const CSqlData<CSqlSaveResult> *pGameData, bool HandleFailure)
{
	const CSqlTeamLoad *pData = dynamic_cast<const CSqlTeamLoad *>(pGameData);
	pData->m_pResult->m_Status = CSqlSaveResult::LOAD_FAILED;

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "lock tables %s_saves write;", pSqlServer->GetPrefix());
		pSqlServer->executeSql(aBuf);
		str_format(aBuf, sizeof(aBuf),
				"SELECT "
					"Savegame, Server, "
					"UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) AS Ago, "
					"(UNHEX(REPLACE(SaveID, '-',''))) AS SaveID "
				"FROM %s_saves "
				"where Code = '%s' AND Map = '%s' AND DDNet7 = false AND Savegame LIKE '%%\\n%s\\t%%';",
				pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr(), pData->m_RequestingPlayer.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() == 0)
		{
			strcpy(pData->m_pResult->m_aMessage, "No such savegame for this map");
			goto end;
		}
		pSqlServer->GetResults()->first();
		auto ServerName = pSqlServer->GetResults()->getString("Server");
		if(str_comp(ServerName.c_str(), g_Config.m_SvSqlServerName) != 0)
		{
			str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
					"You have to be on the '%s' server to load this savegame", ServerName.c_str());
			goto end;
		}

		int Since = pSqlServer->GetResults()->getInt("Ago");
		if(Since < g_Config.m_SvSaveGamesDelay)
		{
			str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
					"You have to wait %d seconds until you can load this savegame",
					g_Config.m_SvSaveGamesDelay - Since);
			goto end;
		}
		if(pSqlServer->GetResults()->isNull("SaveID"))
		{
			memset(pData->m_pResult->m_SaveID.m_aData, 0, sizeof(pData->m_pResult->m_SaveID.m_aData));
		}
		else
		{
			auto SaveID = pSqlServer->GetResults()->getBlob("SaveID");
			SaveID->read((char *) pData->m_pResult->m_SaveID.m_aData, 16);
			if(SaveID->gcount() != 16)
			{
				strcpy(pData->m_pResult->m_aMessage, "Unable to load savegame: SaveID corrupted");
				goto end;
			}
		}

		auto SaveString = pSqlServer->GetResults()->getString("Savegame");
		int Num = pData->m_pResult->m_SavedTeam.LoadString(SaveString.c_str());

		if(Num != 0)
		{
			strcpy(pData->m_pResult->m_aMessage, "Unable to load savegame: data corrupted");
			goto end;
		}

		bool CanLoad = pData->m_pResult->m_SavedTeam.MatchPlayers(
				pData->m_aClientNames, pData->m_aClientID, pData->m_NumPlayer,
				pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage));

		if(!CanLoad)
			goto end;

		str_format(aBuf, sizeof(aBuf),
				"DELETE FROM  %s_saves "
				"WHERE Code='%s' AND Map='%s';",
				pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
		pSqlServer->executeSql(aBuf);

		pData->m_pResult->m_Status = CSqlSaveResult::LOAD_SUCCESS;
		strcpy(pData->m_pResult->m_aMessage, "Loading successfully done");

	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not load the team");
		strcpy(pData->m_pResult->m_aMessage, "MySQL Error: Could not load the team");
		pSqlServer->executeSql("unlock tables;");

		return false;
	}

end:
	pSqlServer->executeSql("unlock tables;");
	return true;
}

void CSqlScore::GetSaves(int ClientID)
{
	ExecPlayerThread(GetSavesThread, "get saves", ClientID, "", 0);
}

bool CSqlScore::GetSavesThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];

		str_format(aBuf, sizeof(aBuf),
				"SELECT COUNT(*) as NumSaves, "
					"UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(max(Timestamp)) as Ago "
				"FROM %s_saves "
				"WHERE Map='%s' AND Savegame LIKE '%%\\n%s\\t%%';",
				pSqlServer->GetPrefix(),
				pData->m_Map.ClrStr(),
				pData->m_RequestingPlayer.ClrStr()
		);
		pSqlServer->executeSqlQuery(aBuf);
		if(pSqlServer->GetResults()->next())
		{
			int NumSaves = pSqlServer->GetResults()->getInt("NumSaves");

			int Ago = pSqlServer->GetResults()->getInt("Ago");
			char aAgoString[40] = "\0";
			char aLastSavedString[60] = "\0";
			if(Ago)
			{
				sqlstr::AgoTimeToString(Ago, aAgoString);
				str_format(aLastSavedString, sizeof(aLastSavedString), ", last saved %s ago", aAgoString);
			}

			str_format(paMessages[0], sizeof(paMessages[0]),
					"%s has %d save%s on %s%s",
					pData->m_RequestingPlayer.Str(),
					NumSaves, NumSaves == 1 ? "" : "s",
					pData->m_Map.Str(), aLastSavedString);
		}

		pData->m_pResult->m_Done = true;
		dbg_msg("sql", "Showing saves done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not get saves");
	}
	return false;
}

#endif
