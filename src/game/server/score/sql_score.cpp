/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore class by Sushi */
#if defined(CONF_SQL)
#include "sql_score.h"

#include <fstream>
#include <cstring>

#include <base/system.h>
#include <engine/server/sql_connector.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/storage.h>

#include "../entities/character.h"
#include "../gamemodes/DDRace.h"
#include "../save.h"

std::atomic_int CSqlScore::ms_InstanceCount(0);

CSqlPlayerResult::CSqlPlayerResult() :
	m_Done(0),
	m_Failed(0),
	m_MessageTarget(DIRECT)
{
	for(int i = 0; i < (int)(sizeof(m_aaMessages)/sizeof(m_aaMessages[0])); i++)
		m_aaMessages[i][0] = 0;
}

CSqlResult::CSqlResult() :
	m_Done(false),
	m_MessageTarget(DIRECT),
	m_TeamMessageTo(-1),
	m_Tag(NONE)
{
	m_Message[0] = 0;
}

CSqlResult::~CSqlResult() {
	switch(m_Tag)
	{
	case NONE:
		break;
	case LOAD:
		//m_Variant.m_LoadTeam.~CSaveTeam();
		break;
	case RANDOM_MAP:
		m_Variant.m_RandomMap.~CRandomMapResult();
		break;
	case MAP_VOTE:
		m_Variant.m_MapVote.~CMapVoteResult();
		break;
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

LOCK CSqlScore::ms_FailureFileLock = lock_create();

CSqlScore::CSqlScore(CGameContext *pGameServer) :
		m_pGameServer(pGameServer),
		m_pServer(pGameServer->Server())
{
	CSqlConnector::ResetReachable();

	//thread_init_and_detach(ExecSqlFunc, new CSqlExecData<CSqlPlayerResult>(Init, new CSqlData<CSqlPlayerResult>(nullptr)), "SqlScore constructor");
}

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

bool CSqlScore::Init(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlData<CSqlPlayerResult>* pData = pGameData;

	if (HandleFailure)
	{
		dbg_msg("sql", "FATAL ERROR: Could not init SqlScore");
		return true;
	}

	try
	{
		char aBuf[1024];
		// get the best time
		str_format(aBuf, sizeof(aBuf), "SELECT Time FROM %s_race WHERE Map='%s' ORDER BY `Time` ASC LIMIT 0, 1;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->next())
		{
			((CGameControllerDDRace*)pData->GameServer()->m_pController)->m_CurrentRecord = (float)pSqlServer->GetResults()->getDouble("Time");

			dbg_msg("sql", "Getting best time on server done");
		}
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Tables were NOT created");
	}
	*/

	return false;
}

void CSqlScore::CheckBirthday(int ClientID)
{
	/*
	CSqlPlayerRequest *Tmp = new CSqlPlayerRequest();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);
	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(CheckBirthdayThread, Tmp), "birthday check");
	*/
}

bool CSqlScore::CheckBirthdayThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/* TODO
	const CSqlPlayerData *pData = dynamic_cast<const CSqlPlayerData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{

		char aBuf[512];

		str_format(aBuf, sizeof(aBuf), "select year(Current) - year(Stamp) as YearsAgo from (select CURRENT_TIMESTAMP as Current, min(Timestamp) as Stamp from %s_race WHERE Name='%s') as l where dayofmonth(Current) = dayofmonth(Stamp) and month(Current) = month(Stamp) and year(Current) > year(Stamp);", pSqlServer->GetPrefix(), pData->m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->next())
		{
			int yearsAgo = pSqlServer->GetResults()->getInt("YearsAgo");
			str_format(aBuf, sizeof(aBuf), "Happy DDNet birthday to %s for finishing their first map %d year%s ago!", pData->m_Name.Str(), yearsAgo, yearsAgo > 1 ? "s" : "");
			pData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);

			str_format(aBuf, sizeof(aBuf), "Happy DDNet birthday, %s!\nYou have finished your first map exactly %d year%s ago!", pData->m_Name.Str(), yearsAgo, yearsAgo > 1 ? "s" : "");

			pData->GameServer()->SendBroadcast(aBuf, pData->m_ClientID);
		}

		dbg_msg("sql", "checking birthday done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL ERROR: %s", e.what());
		dbg_msg("sql", "ERROR: could not check birthday");
	}
	return false;
	*/
	return false;
}

void CSqlScore::LoadScore(int ClientID)
{
	/*
	CSqlPlayerData *Tmp = new CSqlPlayerData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(LoadScoreThread, Tmp), "load score");
	*/
}

// update stuff
bool CSqlScore::LoadScoreThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlPlayerData *pData = dynamic_cast<const CSqlPlayerData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];

		str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_race WHERE Map='%s' AND Name='%s' ORDER BY time ASC LIMIT 1;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);
		if(pSqlServer->GetResults()->next())
		{
			// get the best time
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			pData->PlayerData(pData->m_ClientID)->m_BestTime = Time;
			pData->PlayerData(pData->m_ClientID)->m_CurrentTime = Time;
			if(pData->GameServer()->m_apPlayers[pData->m_ClientID])
			{
				pData->GameServer()->m_apPlayers[pData->m_ClientID]->m_Score = -Time;
				pData->GameServer()->m_apPlayers[pData->m_ClientID]->m_HasFinishScore = true;
			}

			char aColumn[8];
			if(g_Config.m_SvCheckpointSave)
			{
				for(int i = 0; i < NUM_CHECKPOINTS; i++)
				{
					str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
					pData->PlayerData(pData->m_ClientID)->m_aBestCpTime[i] = (float)pSqlServer->GetResults()->getDouble(aColumn);
				}
			}
		}

		dbg_msg("sql", "Getting best time done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update account");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted loading score due to reload/change of map.");
		return true;
	}
	return false;
	*/
	return false;
}

void CSqlScore::MapVote(int ClientID, const char* MapName)
{
	/*
	*ppResult = std::make_shared<CMapVoteResult>();

	CSqlMapVoteData *Tmp = new CSqlMapVoteData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_RequestedMap = MapName;
	Tmp->m_pResult = *ppResult;
	str_copy(Tmp->m_aFuzzyMap, MapName, sizeof(Tmp->m_aFuzzyMap));
	sqlstr::ClearString(Tmp->m_aFuzzyMap, sizeof(Tmp->m_aFuzzyMap));
	sqlstr::FuzzyString(Tmp->m_aFuzzyMap, sizeof(Tmp->m_aFuzzyMap));

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(MapVoteThread, Tmp), "map vote");
	*/
}

bool CSqlScore::MapVoteThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlMapVoteData *pData = dynamic_cast<const CSqlMapVoteData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[768];
		str_format(aBuf, sizeof(aBuf), "SELECT Map, Server FROM %s_maps WHERE Map LIKE '%s' COLLATE utf8mb4_general_ci ORDER BY CASE WHEN Map = '%s' THEN 0 ELSE 1 END, CASE WHEN Map LIKE '%s%%' THEN 0 ELSE 1 END, LENGTH(Map), Map LIMIT 1;", pSqlServer->GetPrefix(), pData->m_aFuzzyMap, pData->m_RequestedMap.ClrStr(), pData->m_RequestedMap.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		CPlayer *pPlayer = pData->GameServer()->m_apPlayers[pData->m_ClientID];

		int64 Now = pData->Server()->Tick();
		int Timeleft = 0;

		if(!pPlayer)
			goto end;

		Timeleft = pPlayer->m_LastVoteCall + pData->Server()->TickSpeed()*g_Config.m_SvVoteDelay - Now;

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(aBuf, sizeof(aBuf), "No map like \"%s\" found. Try adding a '%%' at the start if you don't know the first character. Example: /map %%castle for \"Out of Castle\"", pData->m_RequestedMap.Str());
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		else if(Now < pPlayer->m_FirstVoteTick)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "You must wait %d seconds before making your first vote", (int)((pPlayer->m_FirstVoteTick - Now) / pData->Server()->TickSpeed()) + 1);
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		else if(pPlayer->m_LastVoteCall && Timeleft > 0)
		{
			char aChatmsg[512] = {0};
			str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote", (Timeleft/pData->Server()->TickSpeed())+1);
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aChatmsg);
		}
		else if(time_get() < pData->GameServer()->m_LastMapVote + (time_freq() * g_Config.m_SvVoteMapTimeDelay))
		{
			char chatmsg[512] = {0};
			str_format(chatmsg, sizeof(chatmsg), "There's a %d second delay between map-votes, please wait %d seconds.", g_Config.m_SvVoteMapTimeDelay, (int)(((pData->GameServer()->m_LastMapVote+(g_Config.m_SvVoteMapTimeDelay * time_freq()))/time_freq())-(time_get()/time_freq())));
			pData->GameServer()->SendChatTarget(pData->m_ClientID, chatmsg);
		}
		else
		{
			pSqlServer->GetResults()->next();
			str_copy(pData->m_pResult->m_aMap, pSqlServer->GetResults()->getString("Map").c_str(), sizeof(pData->m_pResult->m_aMap));
			str_copy(pData->m_pResult->m_aServer, pSqlServer->GetResults()->getString("Server").c_str(), sizeof(pData->m_pResult->m_aServer));

			for(char *p = pData->m_pResult->m_aServer; *p; p++)
				*p = tolower(*p);

			pData->m_pResult->m_ClientID = pData->m_ClientID;

			pData->m_pResult->m_Done = true;
		}
		end:
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not start Mapvote");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted mapvote due to reload/change of map.");
		return true;
	}
	return false;
	*/
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
			str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
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

			str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
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
		pData->m_pResult->m_Failed = true;
		pData->m_pResult->m_Done = true;
	}
	return false;
}

void CSqlScore::SaveScore(int ClientID, float Time, const char *pTimestamp, float CpTime[NUM_CHECKPOINTS], bool NotEligible)
{
	/*
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	Tmp->m_NotEligible = NotEligible;
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCpCurrent[i] = CpTime[i];

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(SaveScoreThread, Tmp, false), "save score");
	*/
}

bool CSqlScore::SaveScoreThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
	{
		if (!g_Config.m_SvSqlFailureFile[0])
			return true;

		lock_wait(ms_FailureFileLock);
		IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
		if(File)
		{
			dbg_msg("sql", "ERROR: Could not save Score, writing insert to a file now...");

			char aBuf[768];
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %%s_race(Map, Name, Timestamp, Time, Server, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, GameID, DDNet7) VALUES ('%s', '%s', '%s', '%.2f', '%s', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%s', false);", pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pData->m_aTimestamp, pData->m_Time, g_Config.m_SvSqlServerName, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24], pData->m_GameUuid.ClrStr());
				io_write(File, aBuf, str_length(aBuf));
				io_write_newline(File);
				io_close(File);
				lock_unlock(ms_FailureFileLock);

				pData->GameServer()->SendBroadcast("Database connection failed, score written to a file instead. Admins will add it manually in a few days.", -1);

			return true;
		}
		lock_unlock(ms_FailureFileLock);
		dbg_msg("sql", "ERROR: Could not save Score, NOT even to a file");
		return false;
	}

	if(pData->m_NotEligible)
	{
		return false;
	}

	try
	{
		char aBuf[768];

		str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_race WHERE Map='%s' AND Name='%s' ORDER BY time ASC LIMIT 1;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);
		if(!pSqlServer->GetResults()->next())
		{
			str_format(aBuf, sizeof(aBuf), "SELECT Points FROM %s_maps WHERE Map ='%s'", pSqlServer->GetPrefix(), pData->m_Map.ClrStr());
			pSqlServer->executeSqlQuery(aBuf);

			if(pSqlServer->GetResults()->rowsCount() == 1)
			{
				pSqlServer->GetResults()->next();
				int points = pSqlServer->GetResults()->getInt("Points");
				if (points == 1)
					str_format(aBuf, sizeof(aBuf), "You earned %d point for finishing this map!", points);
				else
					str_format(aBuf, sizeof(aBuf), "You earned %d points for finishing this map!", points);

				try
				{
					pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				}
				catch (CGameContextError &e) {} // just do nothing, it is not much of a problem if the player is not informed about points during mapchange

				str_format(aBuf, sizeof(aBuf), "INSERT INTO %s_points(Name, Points) VALUES ('%s', '%d') ON duplicate key UPDATE Name=VALUES(Name), Points=Points+VALUES(Points);", pSqlServer->GetPrefix(), pData->m_Name.ClrStr(), points);
				pSqlServer->executeSql(aBuf);
			}
		}

		// if no entry found... create a new one
		str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_race(Map, Name, Timestamp, Time, Server, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, GameID, DDNet7) VALUES ('%s', '%s', '%s', '%.2f', '%s', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%s', false);", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pData->m_aTimestamp, pData->m_Time, g_Config.m_SvSqlServerName, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24], pData->m_GameUuid.ClrStr());
		dbg_msg("sql", "%s", aBuf);
		pSqlServer->executeSql(aBuf);

		dbg_msg("sql", "Updating time done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update time");
	}
	return false;
	*/
	return false;
}

void CSqlScore::SaveTeamScore(int* aClientIDs, unsigned int Size, float Time, const char *pTimestamp)
{
	/*
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	CSqlTeamScoreData *Tmp = new CSqlTeamScoreData();
	Tmp->m_NotEligible = false;
	for(unsigned int i = 0; i < Size; i++)
	{
		Tmp->m_aClientIDs[i] = aClientIDs[i];
		Tmp->m_aNames[i] = Server()->ClientName(aClientIDs[i]);
		Tmp->m_NotEligible = Tmp->m_NotEligible || GameServer()->m_apPlayers[aClientIDs[i]]->m_NotEligibleForFinish;
	}
	Tmp->m_Size = Size;
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(SaveTeamScoreThread, Tmp, false), "save team score");
	*/
}

bool CSqlScore::SaveTeamScoreThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlTeamScoreData *pData = dynamic_cast<const CSqlTeamScoreData *>(pGameData);

	if (HandleFailure)
	{
		if (!g_Config.m_SvSqlFailureFile[0])
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
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %%s_teamrace(Map, Name, Timestamp, Time, ID, GameID, DDNet7) VALUES ('%s', '%s', '%s', '%.2f', @id, '%s', false);", pData->m_Map.ClrStr(), pData->m_aNames[i].ClrStr(), pData->m_aTimestamp, pData->m_Time, pData->m_GameUuid.ClrStr());
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

	if(pData->m_NotEligible)
	{
		return false;
	}

	try
	{
		char aBuf[2300];
		char aUpdateID[17];
		aUpdateID[0] = 0;

		str_format(aBuf, sizeof(aBuf), "SELECT Name, l.ID, Time FROM ((SELECT ID FROM %s_teamrace WHERE Map = '%s' AND Name = '%s' and DDNet7 = false) as l) LEFT JOIN %s_teamrace as r ON l.ID = r.ID ORDER BY ID;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_aNames[0].ClrStr(), pSqlServer->GetPrefix());
		pSqlServer->executeSqlQuery(aBuf);

		if (pSqlServer->GetResults()->rowsCount() > 0)
		{
			char aID[17];
			char aID2[17];
			char aName[64];
			unsigned int Count = 0;
			bool ValidNames = true;

			pSqlServer->GetResults()->first();
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			strcpy(aID, pSqlServer->GetResults()->getString("ID").c_str());

			do
			{
				strcpy(aID2, pSqlServer->GetResults()->getString("ID").c_str());
				strcpy(aName, pSqlServer->GetResults()->getString("Name").c_str());
				sqlstr::ClearString(aName);
				if (str_comp(aID, aID2) != 0)
				{
					if (ValidNames && Count == pData->m_Size)
					{
						if (pData->m_Time < Time)
							strcpy(aUpdateID, aID);
						else
							goto end;
						break;
					}

					Time = (float)pSqlServer->GetResults()->getDouble("Time");
					ValidNames = true;
					Count = 0;
					strcpy(aID, aID2);
				}

				if (!ValidNames)
					continue;

				ValidNames = false;

				for(unsigned int i = 0; i < pData->m_Size; i++)
				{
					if (str_comp(aName, pData->m_aNames[i].ClrStr()) == 0)
					{
						ValidNames = true;
						Count++;
						break;
					}
				}
			} while (pSqlServer->GetResults()->next());

			if (ValidNames && Count == pData->m_Size)
			{
				if (pData->m_Time < Time)
					strcpy(aUpdateID, aID);
				else
					goto end;
			}
		}

		if (aUpdateID[0])
		{
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_teamrace SET Time='%.2f', Timestamp='%s', DDNet7=false WHERE ID = '%s';", pSqlServer->GetPrefix(), pData->m_Time, pData->m_aTimestamp, aUpdateID);
			dbg_msg("sql", "%s", aBuf);
			pSqlServer->executeSql(aBuf);
		}
		else
		{
			pSqlServer->executeSql("SET @id = UUID();");

			for(unsigned int i = 0; i < pData->m_Size; i++)
			{
			// if no entry found... create a new one
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_teamrace(Map, Name, Timestamp, Time, ID, GameID, DDNet7) VALUES ('%s', '%s', '%s', '%.2f', @id, '%s', false);", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_aNames[i].ClrStr(), pData->m_aTimestamp, pData->m_Time, pData->m_GameUuid.ClrStr());
				dbg_msg("sql", "%s", aBuf);
				pSqlServer->executeSql(aBuf);
			}
		}

		end:
		dbg_msg("sql", "Updating team time done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update time");
	}
	return false;
	*/
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
			str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
					"%s is not ranked", pData->m_Name.Str());
		}
		else
		{
			pSqlServer->GetResults()->next();

			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = pSqlServer->GetResults()->getInt("Rank");
			if(g_Config.m_SvHideScore)
			{
				str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
						"Your time: %02d:%05.2f", (int)(Time/60), Time-((int)Time/60*60));
			}
			else
			{
				pData->m_pResult->m_MessageTarget = CSqlPlayerResult::ALL;
				str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
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
			str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
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
				str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
						"Your team time: %02d:%05.02f", (int)(Time/60), Time-((int)Time/60*60));
			}
			else
			{
				pData->m_pResult->m_MessageTarget = CSqlPlayerResult::ALL;
				str_format(pData->m_pResult->m_aaMessages[0], sizeof(pData->m_pResult->m_aaMessages[0]),
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
	{
		pData->m_pResult->m_Failed = true;
		pData->m_pResult->m_Done = true;
		return true;
	}

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
		strcpy(pData->m_pResult->m_aaMessages[0], "----------- Top 5 -----------");

		int Line = 1;
		while(pSqlServer->GetResults()->next())
		{
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = pSqlServer->GetResults()->getInt("Rank");
			str_format(pData->m_pResult->m_aaMessages[Line], sizeof(pData->m_pResult->m_aaMessages[0]),
					"%d. %s Time: %02d:%05.2f",
					Rank, pSqlServer->GetResults()->getString("Name").c_str(),
					(int)(Time/60), Time-((int)Time/60*60)
			);
			Line++;
		}
		strcpy(pData->m_pResult->m_aaMessages[Line], "-------------------------------");

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
	auto paMessages = pData->m_pResult->m_aaMessages;
	if (HandleFailure)
	{
		pData->m_pResult->m_Failed = true;
		pData->m_pResult->m_Done = true;
		return true;
	}

	int LimitStart = maximum(abs(pData->m_Offset)-1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	try
	{
		// check sort method
		char aBuf[512];

		pSqlServer->executeSql("SET @pos := 0;");
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
			printf("%d", TeamSize);

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
			str_format(paMessages[Line], sizeof(paMessages[0]), "%d. %s Team Time: %02d:%05.2f",
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
	/*
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Offset;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Search = false;

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(ShowTimesThread, Tmp), "show times");
	*/
}

void CSqlScore::ShowTimes(int ClientID, const char* pName, int Offset)
{
	/*
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Offset;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = pName;
	Tmp->m_Search = true;

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(ShowTimesThread, Tmp), "show name's times");
	*/
}

bool CSqlScore::ShowTimesThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	int LimitStart = maximum(abs(pData->m_Num)-1, 0);
	const char *pOrder = pData->m_Num >= 0 ? "DESC" : "ASC";

	try
	{
		char aBuf[512];

		if(pData->m_Search) // last 5 times of a player
			str_format(aBuf, sizeof(aBuf), "SELECT Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp FROM %s_race WHERE Map = '%s' AND Name = '%s' ORDER BY Timestamp %s LIMIT %d, 5;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pOrder, LimitStart);
		else// last 5 times of server
			str_format(aBuf, sizeof(aBuf), "SELECT Name, Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp FROM %s_race WHERE Map = '%s' ORDER BY Timestamp %s LIMIT %d, 5;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pOrder, LimitStart);

		pSqlServer->executeSqlQuery(aBuf);

		// show top5
		if(pSqlServer->GetResults()->rowsCount() == 0)
		{
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "There are no times in the specified range");
			return true;
		}

		pData->GameServer()->SendChatTarget(pData->m_ClientID, "------------- Last Times -------------");

		float pTime = 0;
		int pSince = 0;
		int pStamp = 0;

		while(pSqlServer->GetResults()->next())
		{
			char aAgoString[40] = "\0";
			pSince = pSqlServer->GetResults()->getInt("Ago");
			pStamp = pSqlServer->GetResults()->getInt("Stamp");
			pTime = (float)pSqlServer->GetResults()->getDouble("Time");

			sqlstr::AgoTimeToString(pSince,aAgoString);

			if(pData->m_Search) // last 5 times of a player
			{
				if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
					str_format(aBuf, sizeof(aBuf), "%02d:%05.02f, don't know how long ago", (int)(pTime/60), pTime-((int)pTime/60*60));
				else
					str_format(aBuf, sizeof(aBuf), "%s ago, %02d:%05.02f", aAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
			}
			else // last 5 times of the server
			{
				if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
					str_format(aBuf, sizeof(aBuf), "%s, %02d:%05.02f, don't know when", pSqlServer->GetResults()->getString("Name").c_str(), (int)(pTime/60), pTime-((int)pTime/60*60));
				else
					str_format(aBuf, sizeof(aBuf), "%s, %s ago, %02d:%05.02f", pSqlServer->GetResults()->getString("Name").c_str(), aAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
			}
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "----------------------------------------------------");

		dbg_msg("sql", "Showing times done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show times");
		return false;
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing times due to reload/change of map.");
		return true;
	}
	*/
	return false;
}

void CSqlScore::ShowPoints(int ClientID, const char* pName)
{
	/*
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = pName;
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(ShowPointsThread, Tmp), "show points");
	*/
}

bool CSqlScore::ShowPointsThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SELECT Rank, Points, Name FROM (SELECT Name, (@pos := @pos+1) pos, (@rank := IF(@prev = Points, @rank, @pos)) Rank, (@prev := Points) Points FROM (SELECT Name, Points FROM %s_points GROUP BY Name ORDER BY Points DESC) as a) as b where Name = '%s';", pSqlServer->GetPrefix(), pData->m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(aBuf, sizeof(aBuf), "%s has not collected any points so far", pData->m_Name.Str());
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		else
		{
			pSqlServer->GetResults()->next();
			int count = pSqlServer->GetResults()->getInt("Points");
			int rank = pSqlServer->GetResults()->getInt("Rank");
			str_format(aBuf, sizeof(aBuf), "%d. %s Points: %d, requested by %s", rank, pSqlServer->GetResults()->getString("Name").c_str(), count, pData->m_aRequestingPlayer);
			pData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);
		}

		dbg_msg("sql", "Showing points done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show points");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing points due to reload/change of map.");
		return true;
	}
	*/
	return false;
}

void CSqlScore::ShowTopPoints(int ClientID, int Offset)
{
	/*
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Offset;
	Tmp->m_ClientID = ClientID;

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(ShowTopPointsThread, Tmp), "show top points");
	*/
}

bool CSqlScore::ShowTopPointsThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	int LimitStart = maximum(abs(pData->m_Num)-1, 0);
	const char *pOrder = pData->m_Num >= 0 ? "ASC" : "DESC";

	try
	{
		char aBuf[512];
		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");
		str_format(aBuf, sizeof(aBuf), "SELECT Rank, Points, Name FROM (SELECT Name, (@pos := @pos+1) pos, (@rank := IF(@prev = Points,@rank, @pos)) Rank, (@prev := Points) Points FROM (SELECT Name, Points FROM %s_points GROUP BY Name ORDER BY Points DESC) as a) as b ORDER BY Rank %s LIMIT %d, 5;", pSqlServer->GetPrefix(), pOrder, LimitStart);

		pSqlServer->executeSqlQuery(aBuf);

		// show top points
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "-------- Top Points --------");

		while(pSqlServer->GetResults()->next())
		{
			str_format(aBuf, sizeof(aBuf), "%d. %s Points: %d", pSqlServer->GetResults()->getInt("Rank"), pSqlServer->GetResults()->getString("Name").c_str(), pSqlServer->GetResults()->getInt("Points"));
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "-------------------------------");

		dbg_msg("sql", "Showing toppoints done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show toppoints");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted toppoints-thread due to reload/change of map.");
		return true;
	}

	*/
	return false;
}

void CSqlScore::RandomMap(int ClientID, int Stars)
{
	/*
	auto pResult = NewSqlResult(ClientID);
	if(pResult == nullptr)
		return;
	CSqlRandomMap *Tmp = new CSqlRandomMap(pResult);
	Tmp->m_Stars = Stars;

	// TODO: Set Client result Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aCurrentMap, m_aMap, sizeof(Tmp->m_aCurrentMap));
	str_copy(Tmp->m_aServerType, g_Config.m_SvServerType, sizeof(Tmp->m_aServerType));

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(RandomMapThread, Tmp), "random map");
	*/
}

bool CSqlScore::RandomMapThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	const CSqlRandomMap *pData = dynamic_cast<const CSqlRandomMap *>(pGameData);

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
					pData->m_aServerType,
					pData->m_aCurrentMap,
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
					pData->m_aServerType,
					pData->m_aCurrentMap
			);
		}
		pSqlServer->executeSqlQuery(aBuf);

		pData->m_pResult->m_Tag = CSqlResult::RANDOM_MAP;
		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			pData->m_pResult->m_MessageTarget = CSqlResult::DIRECT;
			strncpy(pData->m_pResult->m_Message, "No maps found on this server!", sizeof(pData->m_pResult->m_Message));
			pData->m_pResult->m_Variant.m_RandomMap.m_aMap[0] = 0;
		}
		else
		{
			pSqlServer->GetResults()->next();
			std::string Map = pSqlServer->GetResults()->getString("Map");
			str_copy(pData->m_pResult->m_Variant.m_RandomMap.m_aMap, Map.c_str(), sizeof(pData->m_pResult->m_Variant.m_RandomMap.m_aMap));
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
	/*
	*ppResult = std::make_shared<CRandomMapResult>();

	CSqlRandomMap *Tmp = new CSqlRandomMap();
	Tmp->m_Num = Stars;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = GameServer()->Server()->ClientName(ClientID);
	Tmp->m_pResult = *ppResult;

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(RandomUnfinishedMapThread, Tmp), "random unfinished map");
	*/
}

bool CSqlScore::RandomUnfinishedMapThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlRandomMap *pData = dynamic_cast<const CSqlRandomMap *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		if(pData->m_Num >= 0)
			str_format(aBuf, sizeof(aBuf), "select * from %s_maps where Server = \"%s\" and Map != \"%s\" and Stars = \"%d\" and not exists (select * from %s_race where Name = \"%s\" and %s_race.Map = %s_maps.Map) order by RAND() limit 1;", pSqlServer->GetPrefix(), g_Config.m_SvServerType, g_Config.m_SvMap, pData->m_Num, pSqlServer->GetPrefix(), pData->m_Name.ClrStr(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		else
			str_format(aBuf, sizeof(aBuf), "select * from %s_maps where Server = \"%s\" and Map != \"%s\" and not exists (select * from %s_race where Name = \"%s\" and %s_race.Map = %s_maps.Map) order by RAND() limit 1;", pSqlServer->GetPrefix(), g_Config.m_SvServerType, g_Config.m_SvMap, pSqlServer->GetPrefix(), pData->m_Name.ClrStr(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "You have no more unfinished maps on this server!");
			pData->GameServer()->m_LastMapVote = 0;
		}
		else
		{
			pSqlServer->GetResults()->next();
			std::string Map = pSqlServer->GetResults()->getString("Map");
			str_copy(pData->m_pResult->m_aMap, Map.c_str(), sizeof(pData->m_pResult->m_aMap));
			pData->m_pResult->m_Done = true;
		}

		dbg_msg("sql", "voting random unfinished map done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not vote random unfinished map");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted unfinished-map-thread due to reload/change of map.");
		return true;
	}
	return false;
	*/
	return false;
}

void CSqlScore::SaveTeam(int Team, const char* Code, int ClientID, const char* Server)
{
	/*
	if(((CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.GetSaving(Team))
		return;

	CSaveTeam SavedTeam(GameServer()->m_pController);
	int Result = SavedTeam.save(Team);
	if(CSaveTeam::HandleSaveError(Result, ClientID, GameServer()))
		return;
	// disable joining team while saving
	((CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.SetSaving(Team, true);

	CSqlTeamSave *Tmp = new CSqlTeamSave();
	Tmp->m_Team = Team;
	Tmp->m_ClientID = ClientID;
	// copy both save code and save state into the thread struct
	Tmp->m_Code = Code;
	Tmp->m_SaveState = SavedTeam.GetString();
	str_copy(Tmp->m_Server, Server, sizeof(Tmp->m_Server));
	str_copy(Tmp->m_ClientName, this->Server()->ClientName(Tmp->m_ClientID), sizeof(Tmp->m_ClientName));

	// TODO: log event in Teehistorian
	// TODO: find a way to send all players in the team the save code
	((CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.KillSavedTeam(Team);
	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(SaveTeamThread, Tmp, false), "save team");
	*/
}

bool CSqlScore::SaveTeamThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlTeamSave *pData = dynamic_cast<const CSqlTeamSave *>(pGameData);

	try
	{
		if (HandleFailure)
		{
			if (!g_Config.m_SvSqlFailureFile[0])
				return true;

			lock_wait(ms_FailureFileLock);
			IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
			if(File)
			{
				dbg_msg("sql", "ERROR: Could not save Teamsave, writing insert to a file now...");

				char aBuf[65536];
				str_format(aBuf, sizeof(aBuf),
						"INSERT IGNORE INTO %%s_saves(Savegame, Map, Code, Timestamp, Server, DDNet7) "
						"VALUES ('%s', '%s', '%s', CURRENT_TIMESTAMP(), '%s', false);",
						pData->m_SaveState, pData->m_Map.ClrStr(), pData->m_Code.ClrStr(), pData->m_Server
				);
				io_write(File, aBuf, str_length(aBuf));
				io_write_newline(File);
				io_close(File);
				lock_unlock(ms_FailureFileLock);

				pData->GameServer()->SendBroadcast("Database connection failed, teamsave written to a file instead. Admins will add it manually in a few days.", -1);

				return true;
			}
			lock_unlock(ms_FailureFileLock);
			dbg_msg("sql", "ERROR: Could not save Teamsave, NOT even to a file");
			return false;
		}

		try
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "lock tables %s_saves write;", pSqlServer->GetPrefix());
			pSqlServer->executeSql(aBuf);
			str_format(aBuf, sizeof(aBuf), "select Savegame from %s_saves where Code = '%s' and Map = '%s';",  pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
			pSqlServer->executeSqlQuery(aBuf);

			if (pSqlServer->GetResults()->rowsCount() == 0)
			{
				char aBuf[65536];
				str_format(aBuf, sizeof(aBuf),
						"INSERT IGNORE INTO %s_saves(Savegame, Map, Code, Timestamp, Server, DDNet7) VALUES ('%s', '%s', '%s', CURRENT_TIMESTAMP(), '%s', false)",
						pSqlServer->GetPrefix(), pData->m_SaveState.ClrStr(), pData->m_Map.ClrStr(), pData->m_Code.ClrStr(), pData->m_Server
				);
				dbg_msg("sql", "%s", aBuf);
				pSqlServer->executeSql(aBuf);

				// be sure to keep all calls to pData->GameServer() after inserting the save, otherwise it might be lost due to CGameContextError.

				char aBuf2[512];
				str_format(aBuf2, sizeof(aBuf2), "Team successfully saved by %s. Use '/load %s' to continue", pData->m_ClientName, pData->m_Code.Str());
				pData->GameServer()->SendChatTeam(Team, aBuf2);
			}
			else
			{
				dbg_msg("sql", "ERROR: This save-code already exists");
				pData->GameServer()->SendChatTarget(pData->m_ClientID, "This save-code already exists");
			}

		}
		catch (sql::SQLException &e)
		{
			dbg_msg("sql", "MySQL Error: %s", e.what());
			dbg_msg("sql", "ERROR: Could not save the team");
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "MySQL Error: Could not save the team");
			pSqlServer->executeSql("unlock tables;");
			return false;
		}
		catch (CGameContextError &e)
		{
			dbg_msg("sql", "WARNING: Could not send chatmessage during saving team due to reload/change of map.");
		}
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted saving team due to reload/change of map.");
	}

	pSqlServer->executeSql("unlock tables;");
	return true;
	*/
	return false;
}

void CSqlScore::LoadTeam(const char* Code, int ClientID)
{
	/*
	CSqlTeamLoad *Tmp = new CSqlTeamLoad();
	Tmp->m_Code = Code;
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_ClientName, Server()->ClientName(Tmp->m_ClientID), sizeof(Tmp->m_ClientName));

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(LoadTeamThread, Tmp), "load team");
	*/
}

bool CSqlScore::LoadTeamThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlTeamLoad *pData = dynamic_cast<const CSqlTeamLoad *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[768];
		str_format(aBuf, sizeof(aBuf), "lock tables %s_saves write;", pSqlServer->GetPrefix());
		pSqlServer->executeSql(aBuf);
		str_format(aBuf, sizeof(aBuf), "select Savegame, Server, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago from %s_saves where Code = '%s' and Map = '%s' and DDNet7 = false;", pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if (pSqlServer->GetResults()->rowsCount() > 0)
		{
			pSqlServer->GetResults()->first();
			char ServerName[5];
			str_copy(ServerName, pSqlServer->GetResults()->getString("Server").c_str(), sizeof(ServerName));
			if(str_comp(ServerName, g_Config.m_SvSqlServerName))
			{
				str_format(aBuf, sizeof(aBuf), "You have to be on the '%s' server to load this savegame", ServerName);
				pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				goto end;
			}

			pSqlServer->GetResults()->getInt("Ago");
			int since = pSqlServer->GetResults()->getInt("Ago");

			if(since < g_Config.m_SvSaveGamesDelay)
			{
				str_format(aBuf, sizeof(aBuf), "You have to wait %d seconds until you can load this savegame", g_Config.m_SvSaveGamesDelay - since);
				pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				goto end;
			}

			CSaveTeam SavedTeam(pData->GameServer()->m_pController);

			int Num = SavedTeam.LoadString(pSqlServer->GetResults()->getString("Savegame").c_str());

			if(Num)
				pData->GameServer()->SendChatTarget(pData->m_ClientID, "Unable to load savegame: data corrupted");
			else
			{
				bool Found = false;
				for (int i = 0; i < SavedTeam.GetMembersCount(); i++)
				{
					if(str_comp(SavedTeam.m_pSavedTees[i].GetName(), pData->Server()->ClientName(pData->m_ClientID)) == 0)
					{
						Found = true;
						break;
					}
				}
				if(!Found)
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "You don't belong to this team");
				else
				{
					int Team = ((CGameControllerDDRace*)(pData->GameServer()->m_pController))->m_Teams.m_Core.Team(pData->m_ClientID);

					Num = SavedTeam.load(Team);

					if(Num == 1)
					{
						pData->GameServer()->SendChatTarget(pData->m_ClientID, "You have to be in a team (from 1-63)");
					}
					else if(Num == 2)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "Too many players in this team, should be %d", SavedTeam.GetMembersCount());
						pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else if(Num >= 10 && Num < 100)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "Unable to find player: '%s'", SavedTeam.m_pSavedTees[Num-10].GetName());
						pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else if(Num >= 100 && Num < 200)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "%s is racing right now, Team can't be loaded if a Tee is racing already", SavedTeam.m_pSavedTees[Num-100].GetName());
						pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else if(Num >= 200)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "Everyone has to be in a team, %s is in team 0 or the wrong team", SavedTeam.m_pSavedTees[Num-200].GetName());
						pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else
					{
						char aBuf[512];
						str_format(aBuf, sizeof(aBuf), "Loading successfully done by %s", pData->m_ClientName);
						pData->GameServer()->SendChatTeam(Team, aBuf);
						str_format(aBuf, sizeof(aBuf), "DELETE from %s_saves where Code='%s' and Map='%s';", pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
						pSqlServer->executeSql(aBuf);
					}
				}
			}
		}
		else
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "No such savegame for this map");

		end:
		pSqlServer->executeSql("unlock tables;");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not load the team");
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "MySQL Error: Could not load the team");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted loading team due to reload/change of map.");
		return true;
	}
	return false;
	*/
	return false;
}

void CSqlScore::GetSaves(int ClientID)
{
	/*
	CSqlGetSavesData *Tmp = new CSqlGetSavesData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);

	thread_init_and_detach(ExecSqlFunc, new CSqlExecData(GetSavesThread, Tmp, false), "get saves");
	*/
}

bool CSqlScore::GetSavesThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure)
{
	/*
	const CSqlGetSavesData *pData = dynamic_cast<const CSqlGetSavesData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];

		str_format(aBuf, sizeof(aBuf), "SELECT count(*) as NumSaves, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(max(Timestamp)) as Ago FROM %s_saves WHERE Map='%s' AND Savegame regexp '\\n%s\\t';", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr());
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

			str_format(pData->m_pResult->m_Message,
					sizeof(pData->m_pResult->m_Message),
					"%s has %d save%s on %s%s", pData->m_Name.Str(), NumSaves, NumSaves == 1 ? "" : "s", pData->m_Map.Str(), aLastSavedString);
		}

		dbg_msg("sql", "Showing saves done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not get saves");
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "MySQL Error: Could not get saves");
	}
	return false;
	*/
	return false;
}

#endif
