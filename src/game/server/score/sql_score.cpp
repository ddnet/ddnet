/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore class by Sushi */
#if defined(CONF_SQL)
#include <fstream>
#include <cstring>

#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/storage.h>

#include "sql_score.h"

#include "../entities/character.h"
#include "../gamemodes/DDRace.h"
#include "../save.h"

CGameContext* CSqlData::ms_pGameServer = 0;
IServer* CSqlData::ms_pServer = 0;
CPlayerData* CSqlData::ms_pPlayerData = 0;
const char* CSqlData::ms_pMap = 0;

bool CSqlData::ms_GameContextAvailable = false;
int CSqlData::ms_Instance = 0;

int CSqlExecData::ms_InstanceCount = 0;

LOCK CSqlScore::ms_FailureFileLock = lock_create();

CSqlTeamSave::~CSqlTeamSave()
{
	try
	{
		((class CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.SetSaving(m_Team, false);
	}
	catch (CGameContextError& e) {}
}



CSqlScore::CSqlScore(CGameContext *pGameServer) :
m_pGameServer(pGameServer),
m_pServer(pGameServer->Server())
{
	str_copy(m_aMap, g_Config.m_SvMap, sizeof(m_aMap));

	CSqlData::ms_pGameServer = m_pGameServer;
	CSqlData::ms_pServer = m_pServer;
	CSqlData::ms_pPlayerData = PlayerData(0);
	CSqlData::ms_pMap = m_aMap;

	CSqlData::ms_GameContextAvailable = true;
	++CSqlData::ms_Instance;

	CSqlConnector::ResetReachable();

	void* InitThread = thread_init(ExecSqlFunc, new CSqlExecData(Init, new CSqlData()));
	thread_detach(InitThread);
}


CSqlScore::~CSqlScore()
{
	CSqlData::ms_GameContextAvailable = false;
}

void CSqlScore::OnShutdown()
{
	CSqlData::ms_GameContextAvailable = false;
	int i = 0;
	while (CSqlExecData::ms_InstanceCount != 0)
	{
		// print a log about every two seconds
		if (i % 20 == 0)
			dbg_msg("sql", "Waiting for score-threads to complete (%d left)", CSqlExecData::ms_InstanceCount);
		++i;
		thread_sleep(100);
	}

	lock_destroy(ms_FailureFileLock);
}

void CSqlScore::ExecSqlFunc(void *pUser)
{
	CSqlExecData* pData = (CSqlExecData *)pUser;

	CSqlConnector connector;

	bool Success = false;

	// try to connect to a working databaseserver
	while (!Success && !connector.MaxTriesReached(pData->m_ReadOnly) && connector.ConnectSqlServer(pData->m_ReadOnly))
	{
		if (pData->m_pFuncPtr(connector.SqlServer(), pData->m_pSqlData, false))
			Success = true;

		// disconnect from databaseserver
		connector.SqlServer()->Disconnect();
	}

	// handle failures
	// eg write inserts to a file and print a nice error message
	if (!Success)
		pData->m_pFuncPtr(0, pData->m_pSqlData, true);

	delete pData->m_pSqlData;
	delete pData;
}

bool CSqlScore::Init(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlData* pData = pGameData;

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
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Setting best time failed (game reloaded).");
	}

	return false;
}

void CSqlScore::CheckBirthday(int ClientID)
{
	CSqlPlayerData *Tmp = new CSqlPlayerData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);
	void *CheckThread = thread_init(ExecSqlFunc, new CSqlExecData(CheckBirthdayThread, Tmp));
	thread_detach(CheckThread);
}

bool CSqlScore::CheckBirthdayThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
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
			int yearsAgo = (int)pSqlServer->GetResults()->getInt("YearsAgo");
			str_format(aBuf, sizeof(aBuf), "Happy DDNet birthday to %s for finishing their first map %d year%s ago!", pData->m_Name.Str(), yearsAgo, yearsAgo > 1 ? "s" : "");
			pData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);

			str_format(aBuf, sizeof(aBuf), "Happy DDNet birthday %s !\nYou have finished your first map %d year%s ago!", pData->m_Name.Str(), yearsAgo, yearsAgo > 1 ? "s" : "");

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
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted checking ddnet-birthday due to reload/change of map.");
		return true;
	}

	return false;
}

void CSqlScore::LoadScore(int ClientID)
{
	CSqlPlayerData *Tmp = new CSqlPlayerData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);

	void *LoadThread = thread_init(ExecSqlFunc, new CSqlExecData(LoadScoreThread, Tmp));
	thread_detach(LoadThread);
}

// update stuff
bool CSqlScore::LoadScoreThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
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
				pData->GameServer()->m_apPlayers[pData->m_ClientID]->m_Score = -Time;

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
}

void CSqlScore::MapVote(int ClientID, const char* MapName)
{
	CSqlMapData *Tmp = new CSqlMapData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_RequestedMap = MapName;
	str_copy(Tmp->m_aFuzzyMap, MapName, sizeof(Tmp->m_aFuzzyMap));
	sqlstr::ClearString(Tmp->m_aFuzzyMap, sizeof(Tmp->m_aFuzzyMap));
	sqlstr::FuzzyString(Tmp->m_aFuzzyMap, sizeof(Tmp->m_aFuzzyMap));

	void *VoteThread = thread_init(ExecSqlFunc, new CSqlExecData(MapVoteThread, Tmp));
	thread_detach(VoteThread);
}

bool CSqlScore::MapVoteThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlMapData *pData = dynamic_cast<const CSqlMapData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[768];
		str_format(aBuf, sizeof(aBuf), "SELECT Map, Server FROM %s_maps WHERE Map LIKE '%s' COLLATE utf8_general_ci ORDER BY CASE WHEN Map = '%s' THEN 0 ELSE 1 END, CASE WHEN Map LIKE '%s%%' THEN 0 ELSE 1 END, LENGTH(Map), Map LIMIT 1;", pSqlServer->GetPrefix(), pData->m_aFuzzyMap, pData->m_RequestedMap.ClrStr(), pData->m_RequestedMap.ClrStr());
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
			str_format(aBuf, sizeof(aBuf), "You must wait %d seconds before making your first vote", ((pPlayer->m_FirstVoteTick - Now) / pData->Server()->TickSpeed()) + 1);
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
			str_format(chatmsg, sizeof(chatmsg), "There's a %d second delay between map-votes, please wait %d seconds.", g_Config.m_SvVoteMapTimeDelay,((pData->GameServer()->m_LastMapVote+(g_Config.m_SvVoteMapTimeDelay * time_freq()))/time_freq())-(time_get()/time_freq()));
			pData->GameServer()->SendChatTarget(pData->m_ClientID, chatmsg);
		}
		else
		{
			pSqlServer->GetResults()->next();
			char aMap[128];
			strcpy(aMap, pSqlServer->GetResults()->getString("Map").c_str());
			char aServer[32];
			strcpy(aServer, pSqlServer->GetResults()->getString("Server").c_str());

			for(char *p = aServer; *p; p++)
				*p = tolower(*p);

			char aCmd[256];
			str_format(aCmd, sizeof(aCmd), "sv_reset_file types/%s/flexreset.cfg; change_map \"%s\"", aServer, aMap);
			char aChatmsg[512];
			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", pData->GameServer()->Server()->ClientName(pData->m_ClientID), aMap, "/map");

			pData->GameServer()->m_VoteKick = false;
			pData->GameServer()->m_VoteSpec = false;
			pData->GameServer()->m_LastMapVote = time_get();
			pData->GameServer()->CallVote(pData->m_ClientID, aMap, aCmd, "/map", aChatmsg);
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
}

void CSqlScore::MapInfo(int ClientID, const char* MapName)
{
	CSqlMapData *Tmp = new CSqlMapData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_RequestedMap = MapName;
	str_copy(Tmp->m_aFuzzyMap, MapName, sizeof(Tmp->m_aFuzzyMap));
	sqlstr::ClearString(Tmp->m_aFuzzyMap, sizeof(Tmp->m_aFuzzyMap));
	sqlstr::FuzzyString(Tmp->m_aFuzzyMap, sizeof(Tmp->m_aFuzzyMap));

	void *InfoThread = thread_init(ExecSqlFunc, new CSqlExecData(MapInfoThread, Tmp));
	thread_detach(InfoThread);
}

bool CSqlScore::MapInfoThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlMapData *pData = dynamic_cast<const CSqlMapData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "SELECT l.Map, l.Server, Mapper, Points, Stars, (select count(Name) from %s_race where Map = l.Map) as Finishes, (select count(distinct Name) from %s_race where Map = l.Map) as Finishers, (select round(avg(Time)) from %s_race where Map = l.Map) as Average, UNIX_TIMESTAMP(l.Timestamp) as Stamp, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(l.Timestamp) as Ago FROM (SELECT * FROM %s_maps WHERE Map LIKE '%s' COLLATE utf8_general_ci ORDER BY CASE WHEN Map = '%s' THEN 0 ELSE 1 END, CASE WHEN Map LIKE '%s%%' THEN 0 ELSE 1 END, LENGTH(Map), Map LIMIT 1) as l;", pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pData->m_aFuzzyMap, pData->m_RequestedMap.ClrStr(), pData->m_RequestedMap.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(aBuf, sizeof(aBuf), "No map like \"%s\" found.", pData->m_RequestedMap.Str());
		}
		else
		{
			pSqlServer->GetResults()->next();
			int points = (int)pSqlServer->GetResults()->getInt("Points");
			int stars = (int)pSqlServer->GetResults()->getInt("Stars");
			int finishes = (int)pSqlServer->GetResults()->getInt("Finishes");
			int finishers = (int)pSqlServer->GetResults()->getInt("Finishers");
			int average = (int)pSqlServer->GetResults()->getInt("Average");
			char aMap[128];
			strcpy(aMap, pSqlServer->GetResults()->getString("Map").c_str());
			char aServer[32];
			strcpy(aServer, pSqlServer->GetResults()->getString("Server").c_str());
			char aMapper[128];
			strcpy(aMapper, pSqlServer->GetResults()->getString("Mapper").c_str());
			int stamp = (int)pSqlServer->GetResults()->getInt("Stamp");
			int ago = (int)pSqlServer->GetResults()->getInt("Ago");

			char pAgoString[40] = "\0";
			char pReleasedString[60] = "\0";
			if(stamp != 0)
			{
				sqlstr::AgoTimeToString(ago, pAgoString);
				str_format(pReleasedString, sizeof(pReleasedString), ", released %s ago", pAgoString);
			}

			char pAverageString[60] = "\0";
			if(average > 0)
			{
				str_format(pAverageString, sizeof(pAverageString), " in %d:%02d average", average / 60, average % 60);
			}

			char aStars[20];
			switch(stars)
			{
				case 0: strcpy(aStars, "✰✰✰✰✰"); break;
				case 1: strcpy(aStars, "★✰✰✰✰"); break;
				case 2: strcpy(aStars, "★★✰✰✰"); break;
				case 3: strcpy(aStars, "★★★✰✰"); break;
				case 4: strcpy(aStars, "★★★★✰"); break;
				case 5: strcpy(aStars, "★★★★★"); break;
				default: aStars[0] = '\0';
			}

			str_format(aBuf, sizeof(aBuf), "\"%s\" by %s on %s (%s, %d %s, %d %s by %d %s%s%s)", aMap, aMapper, aServer, aStars, points, points == 1 ? "point" : "points", finishes, finishes == 1 ? "finish" : "finishes", finishers, finishers == 1 ? "tee" : "tees", pAverageString, pReleasedString);
		}

		pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not get Mapinfo");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted mapinfo-thread due to reload/change of map.");
		return true;
	}
	return false;
}

void CSqlScore::SaveScore(int ClientID, float Time, float CpTime[NUM_CHECKPOINTS])
{
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = Server()->ClientName(ClientID);
	Tmp->m_Time = Time;
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCpCurrent[i] = CpTime[i];

	void *SaveThread = thread_init(ExecSqlFunc, new CSqlExecData(SaveScoreThread, Tmp, false));
	thread_detach(SaveThread);
}

bool CSqlScore::SaveScoreThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
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

			char aTimestamp [20];
			sqlstr::GetTimeStamp(aTimestamp, sizeof(aTimestamp));

			char aBuf[768];
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %%s_race(Map, Name, Timestamp, Time, Server, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', '%s', '%.2f', '%s', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f');", pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), aTimestamp, pData->m_Time, g_Config.m_SvSqlServerName, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24]);
				io_write(File, aBuf, str_length(aBuf));
				io_write_newline(File);
				io_close(File);
				lock_unlock(ms_FailureFileLock);

			return true;
		}
		lock_unlock(ms_FailureFileLock);
		dbg_msg("sql", "ERROR: Could not save Score, NOT even to a file");
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
				int points = (int)pSqlServer->GetResults()->getInt("Points");
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
		str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_race(Map, Name, Timestamp, Time, Server, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', CURRENT_TIMESTAMP(), '%.2f', '%s', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f');", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pData->m_Time, g_Config.m_SvSqlServerName, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24]);
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
}

void CSqlScore::SaveTeamScore(int* aClientIDs, unsigned int Size, float Time)
{
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	CSqlTeamScoreData *Tmp = new CSqlTeamScoreData();
	for(unsigned int i = 0; i < Size; i++)
	{
		Tmp->m_aClientIDs[i] = aClientIDs[i];
		Tmp->m_aNames[i] = Server()->ClientName(aClientIDs[i]);
	}
	Tmp->m_Size = Size;
	Tmp->m_Time = Time;

	void *SaveTeamThread = thread_init(ExecSqlFunc, new CSqlExecData(SaveTeamScoreThread, Tmp, false));
	thread_detach(SaveTeamThread);
}

bool CSqlScore::SaveTeamScoreThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
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
			const char pUUID[] = "SET @id = UUID();";
			io_write(File, pUUID, sizeof(pUUID) - 1);
			io_write_newline(File);

			char aTimestamp [20];
			sqlstr::GetTimeStamp(aTimestamp, sizeof(aTimestamp));

			char aBuf[2300];
			for(unsigned int i = 0; i < pData->m_Size; i++)
			{
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %%s_teamrace(Map, Name, Timestamp, Time, ID) VALUES ('%s', '%s', '%s', '%.2f', @id);", pData->m_Map.ClrStr(), pData->m_aNames[i].ClrStr(), aTimestamp, pData->m_Time);
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
		char aUpdateID[17];
		aUpdateID[0] = 0;

		str_format(aBuf, sizeof(aBuf), "SELECT Name, l.ID, Time FROM ((SELECT ID FROM %s_teamrace WHERE Map = '%s' AND Name = '%s') as l) LEFT JOIN %s_teamrace as r ON l.ID = r.ID ORDER BY ID;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_aNames[0].ClrStr(), pSqlServer->GetPrefix());
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
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_teamrace SET Time='%.2f' WHERE ID = '%s';", pSqlServer->GetPrefix(), pData->m_Time, aUpdateID);
			dbg_msg("sql", "%s", aBuf);
			pSqlServer->executeSql(aBuf);
		}
		else
		{
			pSqlServer->executeSql("SET @id = UUID();");

			for(unsigned int i = 0; i < pData->m_Size; i++)
			{
			// if no entry found... create a new one
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_teamrace(Map, Name, Timestamp, Time, ID) VALUES ('%s', '%s', CURRENT_TIMESTAMP(), '%.2f', @id);", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_aNames[i].ClrStr(), pData->m_Time);
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
}

void CSqlScore::ShowRank(int ClientID, const char* pName, bool Search)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = pName;
	Tmp->m_Search = Search;
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));

	void *RankThread = thread_init(ExecSqlFunc, new CSqlExecData(ShowRankThread, Tmp));
	thread_detach(RankThread);
}

bool CSqlScore::ShowRankThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		// check sort methode
		char aBuf[600];

		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");
		str_format(aBuf, sizeof(aBuf), "SELECT Rank, Name, Time FROM (SELECT Name, (@pos := @pos+1) pos, (@rank := IF(@prev = Time,@rank, @pos)) rank, (@prev := Time) Time FROM (SELECT Name, min(Time) as Time FROM %s_race WHERE Map = '%s' GROUP BY Name ORDER BY `Time` ASC) as a) as b WHERE Name = '%s';", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr());

		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_Name.Str());
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		else
		{
			pSqlServer->GetResults()->next();

			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = (int)pSqlServer->GetResults()->getInt("Rank");
			if(g_Config.m_SvHideScore)
			{
				str_format(aBuf, sizeof(aBuf), "Your time: %02d:%05.2f", (int)(Time/60), Time-((int)Time/60*60));
				pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %02d:%05.2f, requested by %s", Rank, pSqlServer->GetResults()->getString("Name").c_str(), (int)(Time/60), Time-((int)Time/60*60), pData->m_aRequestingPlayer);
				pData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);
			}
		}

		dbg_msg("sql", "Showing rank done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show rank");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing rank due to reload/change of map.");
		return true;
	}
	return false;
}

void CSqlScore::ShowTeamRank(int ClientID, const char* pName, bool Search)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = pName;
	Tmp->m_Search = Search;
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));

	void *TeamRankThread = thread_init(ExecSqlFunc, new CSqlExecData(ShowTeamRankThread, Tmp));
	thread_detach(TeamRankThread);
}

bool CSqlScore::ShowTeamRankThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		// check sort methode
		char aBuf[2400];
		char aNames[2300];
		aNames[0] = '\0';

		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");
		str_format(aBuf, sizeof(aBuf), "SELECT Rank, Name, Time FROM (SELECT Rank, l2.ID FROM ((SELECT ID, (@pos := @pos+1) pos, (@rank := IF(@prev = Time,@rank,@pos)) rank, (@prev := Time) Time FROM (SELECT ID, Time FROM %s_teamrace WHERE Map = '%s' GROUP BY ID ORDER BY Time) as ll) as l2) LEFT JOIN %s_teamrace as r2 ON l2.ID = r2.ID WHERE Map = '%s' AND Name = '%s' ORDER BY Rank LIMIT 1) as l LEFT JOIN %s_teamrace as r ON l.ID = r.ID ORDER BY Name;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pSqlServer->GetPrefix());

		pSqlServer->executeSqlQuery(aBuf);

		int Rows = pSqlServer->GetResults()->rowsCount();

		if(Rows < 1)
		{
			str_format(aBuf, sizeof(aBuf), "%s has no team ranks", pData->m_Name.Str());
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		else
		{
			pSqlServer->GetResults()->first();

			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			int Rank = (int)pSqlServer->GetResults()->getInt("Rank");

			for(int Row = 0; Row < Rows; Row++)
			{
				str_append(aNames, pSqlServer->GetResults()->getString("Name").c_str(), sizeof(aNames));
				pSqlServer->GetResults()->next();

				if (Row < Rows - 2)
					str_append(aNames, ", ", sizeof(aNames));
				else if (Row < Rows - 1)
					str_append(aNames, " & ", sizeof(aNames));
			}

			pSqlServer->GetResults()->first();

			if(g_Config.m_SvHideScore)
			{
				str_format(aBuf, sizeof(aBuf), "Your team time: %02d:%05.02f", (int)(Time/60), Time-((int)Time/60*60));
				pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "%d. %s Team time: %02d:%05.02f, requested by %s", Rank, aNames, (int)(Time/60), Time-((int)Time/60*60), pData->m_aRequestingPlayer);
				pData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);
			}
		}

		dbg_msg("sql", "Showing teamrank done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show team rank");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing teamrank due to reload/change of map.");
		return true;
	}
	return false;
}

void CSqlScore::ShowTop5(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;

	void *Top5Thread = thread_init(ExecSqlFunc, new CSqlExecData(ShowTop5Thread, Tmp));
	thread_detach(Top5Thread);
}

bool CSqlScore::ShowTop5Thread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		// check sort methode
		char aBuf[512];
		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");
		str_format(aBuf, sizeof(aBuf), "SELECT Name, Time, rank FROM (SELECT Name, (@pos := @pos+1) pos, (@rank := IF(@prev = Time,@rank, @pos)) rank, (@prev := Time) Time FROM (SELECT Name, min(Time) as Time FROM %s_race WHERE Map = '%s' GROUP BY Name ORDER BY `Time` ASC) as a) as b LIMIT %d, 5;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Num-1);
		pSqlServer->executeSqlQuery(aBuf);

		// show top5
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "----------- Top 5 -----------");

		int Rank = 0;
		float Time = 0;
		while(pSqlServer->GetResults()->next())
		{
			Time = (float)pSqlServer->GetResults()->getDouble("Time");
			Rank = (float)pSqlServer->GetResults()->getInt("rank");
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %02d:%05.2f", Rank, pSqlServer->GetResults()->getString("Name").c_str(), (int)(Time/60), Time-((int)Time/60*60));
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			//Rank++;
		}
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "-------------------------------");

		dbg_msg("sql", "Showing top5 done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show top5");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing top5 due to reload/change of map.");
		return true;
	}
	return false;
}

void CSqlScore::ShowTeamTop5(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;

	void *TeamTop5Thread = thread_init(ExecSqlFunc, new CSqlExecData(ShowTeamTop5Thread, Tmp));
	thread_detach(TeamTop5Thread);
}

bool CSqlScore::ShowTeamTop5Thread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		// check sort methode
		char aBuf[2400];

		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @previd := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");
		str_format(aBuf, sizeof(aBuf), "SELECT ID, Name, Time, rank FROM (SELECT r.ID, Name, rank, l.Time FROM ((SELECT ID, rank, Time FROM (SELECT ID, (@pos := IF(@previd = ID,@pos,@pos+1)) pos, (@previd := ID), (@rank := IF(@prev = Time,@rank,@pos)) rank, (@prev := Time) Time FROM (SELECT ID, MIN(Time) as Time FROM %s_teamrace WHERE Map = '%s' GROUP BY ID ORDER BY `Time` ASC) as all_top_times) as a LIMIT %d, 5) as l) LEFT JOIN %s_teamrace as r ON l.ID = r.ID ORDER BY Time ASC, r.ID, Name ASC) as a;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Num-1, pSqlServer->GetPrefix(), pData->m_Map.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		// show teamtop5
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "------- Team Top 5 -------");

		int Rows = pSqlServer->GetResults()->rowsCount();

		if (Rows >= 1)
		{
			char aID[17];
			char aID2[17];
			char aNames[2300];
			int Rank = 0;
			float Time = 0;
			int aCuts[320]; // 64 * 5
			int CutPos = 0;

			aNames[0] = '\0';
			aCuts[0] = -1;

			pSqlServer->GetResults()->first();
			strcpy(aID, pSqlServer->GetResults()->getString("ID").c_str());
			for(int Row = 0; Row < Rows; Row++)
			{
				strcpy(aID2, pSqlServer->GetResults()->getString("ID").c_str());
				if (str_comp(aID, aID2) != 0)
				{
					strcpy(aID, aID2);
					aCuts[CutPos++] = Row - 1;
				}
				pSqlServer->GetResults()->next();
			}
			aCuts[CutPos] = Rows - 1;

			CutPos = 0;
			pSqlServer->GetResults()->first();
			for(int Row = 0; Row < Rows; Row++)
			{
				str_append(aNames, pSqlServer->GetResults()->getString("Name").c_str(), sizeof(aNames));

				if (Row < aCuts[CutPos] - 1)
					str_append(aNames, ", ", sizeof(aNames));
				else if (Row < aCuts[CutPos])
					str_append(aNames, " & ", sizeof(aNames));

				Time = (float)pSqlServer->GetResults()->getDouble("Time");
				Rank = (float)pSqlServer->GetResults()->getInt("rank");

				if (Row == aCuts[CutPos])
				{
					str_format(aBuf, sizeof(aBuf), "%d. %s Team Time: %02d:%05.2f", Rank, aNames, (int)(Time/60), Time-((int)Time/60*60));
					pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					CutPos++;
					aNames[0] = '\0';
				}

				pSqlServer->GetResults()->next();
			}
		}

		pData->GameServer()->SendChatTarget(pData->m_ClientID, "-------------------------------");

		dbg_msg("sql", "Showing teamtop5 done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not show teamtop5");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing teamtop5 due to reload/change of map.");
		return true;
	}
	return false;
}

void CSqlScore::ShowTimes(int ClientID, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Search = false;

	void *TimesThread = thread_init(ExecSqlFunc, new CSqlExecData(ShowTimesThread, Tmp));
	thread_detach(TimesThread);
}

void CSqlScore::ShowTimes(int ClientID, const char* pName, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = pName;
	Tmp->m_Search = true;

	void *TimesThread = thread_init(ExecSqlFunc, new CSqlExecData(ShowTimesThread, Tmp));
	thread_detach(TimesThread);
}

bool CSqlScore::ShowTimesThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];

		if(pData->m_Search) // last 5 times of a player
			str_format(aBuf, sizeof(aBuf), "SELECT Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp FROM %s_race WHERE Map = '%s' AND Name = '%s' ORDER BY Ago ASC LIMIT %d, 5;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Name.ClrStr(), pData->m_Num-1);
		else// last 5 times of server
			str_format(aBuf, sizeof(aBuf), "SELECT Name, Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp FROM %s_race WHERE Map = '%s' ORDER BY Ago ASC LIMIT %d, 5;", pSqlServer->GetPrefix(), pData->m_Map.ClrStr(), pData->m_Num-1);

		pSqlServer->executeSqlQuery(aBuf);

		// show top5
		if(pSqlServer->GetResults()->rowsCount() == 0)
		{
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "There are no times in the specified range");
			goto end;
		}

		str_format(aBuf, sizeof(aBuf), "------------ Last Times No %d - %d ------------",pData->m_Num,pData->m_Num + pSqlServer->GetResults()->rowsCount() - 1);
		pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);

		float pTime = 0;
		int pSince = 0;
		int pStamp = 0;

		while(pSqlServer->GetResults()->next())
		{
			char pAgoString[40] = "\0";
			pSince = (int)pSqlServer->GetResults()->getInt("Ago");
			pStamp = (int)pSqlServer->GetResults()->getInt("Stamp");
			pTime = (float)pSqlServer->GetResults()->getDouble("Time");

			sqlstr::AgoTimeToString(pSince,pAgoString);

			if(pData->m_Search) // last 5 times of a player
			{
				if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
					str_format(aBuf, sizeof(aBuf), "%02d:%05.02f, don't know how long ago", (int)(pTime/60), pTime-((int)pTime/60*60));
				else
					str_format(aBuf, sizeof(aBuf), "%s ago, %02d:%05.02f", pAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
			}
			else // last 5 times of the server
			{
				if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
					str_format(aBuf, sizeof(aBuf), "%s, %02d:%05.02f, don't know when", pSqlServer->GetResults()->getString("Name").c_str(), (int)(pTime/60), pTime-((int)pTime/60*60));
				else
					str_format(aBuf, sizeof(aBuf), "%s, %s ago, %02d:%05.02f", pSqlServer->GetResults()->getString("Name").c_str(), pAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
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
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted showing times due to reload/change of map.");
		return true;
	}

	end:
	return false;
}

void CSqlScore::ShowPoints(int ClientID, const char* pName, bool Search)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = pName;
	Tmp->m_Search = Search;
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));

	void *PointsThread = thread_init(ExecSqlFunc, new CSqlExecData(ShowPointsThread, Tmp));
	thread_detach(PointsThread);
}

bool CSqlScore::ShowPointsThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "select Rank, Name, Points from (select (@pos := @pos+1) pos, (@rank := IF(@prev = Points,@rank,@pos)) Rank, Points, Name from (select (@prev := Points) Points, Name from %s_points order by Points desc) as ll) as l where Name = '%s';", pSqlServer->GetPrefix(), pData->m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			str_format(aBuf, sizeof(aBuf), "%s has not collected any points so far", pData->m_Name.Str());
			pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		}
		else
		{
			pSqlServer->GetResults()->next();
			int count = (int)pSqlServer->GetResults()->getInt("Points");
			int rank = (int)pSqlServer->GetResults()->getInt("rank");
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
	return false;
}

void CSqlScore::ShowTopPoints(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;

	void *TopPointsThread = thread_init(ExecSqlFunc, new CSqlExecData(ShowTopPointsThread, Tmp));
	thread_detach(TopPointsThread);
}

bool CSqlScore::ShowTopPointsThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		pSqlServer->executeSql("SET @prev := NULL;");
		pSqlServer->executeSql("SET @rank := 1;");
		pSqlServer->executeSql("SET @pos := 0;");
		str_format(aBuf, sizeof(aBuf), "select Rank, Name, Points from (select (@pos := @pos+1) pos, (@rank := IF(@prev = Points,@rank,@pos)) Rank, Points, Name from (select (@prev := Points) Points, Name from %s_points order by Points desc) as ll) as l LIMIT %d, 5;", pSqlServer->GetPrefix(), pData->m_Num-1);

		pSqlServer->executeSqlQuery(aBuf);

		// show top points
		pData->GameServer()->SendChatTarget(pData->m_ClientID, "-------- Top Points --------");

		while(pSqlServer->GetResults()->next())
		{
			str_format(aBuf, sizeof(aBuf), "%d. %s Points: %d", pSqlServer->GetResults()->getInt("rank"), pSqlServer->GetResults()->getString("Name").c_str(), pSqlServer->GetResults()->getInt("Points"));
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

	return false;
}

void CSqlScore::RandomMap(int ClientID, int stars)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = stars;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = GameServer()->Server()->ClientName(ClientID);

	void *RandomThread = thread_init(ExecSqlFunc, new CSqlExecData(RandomMapThread, Tmp));
	thread_detach(RandomThread);
}

bool CSqlScore::RandomMapThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		if(pData->m_Num)
			str_format(aBuf, sizeof(aBuf), "select * from %s_maps where Server = \"%s\" and Stars = \"%d\" order by RAND() limit 1;", pSqlServer->GetPrefix(), g_Config.m_SvServerType, pData->m_Num);
		else
			str_format(aBuf, sizeof(aBuf), "select * from %s_maps where Server = \"%s\" order by RAND() limit 1;", pSqlServer->GetPrefix(), g_Config.m_SvServerType);
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "No maps found on this server!");
		}
		else
		{
			pSqlServer->GetResults()->next();
			char aMap[128];
			strcpy(aMap, pSqlServer->GetResults()->getString("Map").c_str());

			str_format(aBuf, sizeof(aBuf), "change_map \"%s\"", aMap);
			pData->GameServer()->Console()->ExecuteLine(aBuf);
		}

		dbg_msg("sql", "voting random map done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not vote random map");
	}
	catch (CGameContextError &e)
	{
		dbg_msg("sql", "WARNING: Aborted random-map-thread due to reload/change of map.");
		return true;
	}
	return false;
}

void CSqlScore::RandomUnfinishedMap(int ClientID, int stars)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = stars;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Name = GameServer()->Server()->ClientName(ClientID);

	void *RandomUnfinishedThread = thread_init(ExecSqlFunc, new CSqlExecData(RandomUnfinishedMapThread, Tmp));
	thread_detach(RandomUnfinishedThread);
}

bool CSqlScore::RandomUnfinishedMapThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[512];
		if(pData->m_Num)
			str_format(aBuf, sizeof(aBuf), "select * from %s_maps where Server = \"%s\" and Stars = \"%d\" and not exists (select * from %s_race where Name = \"%s\" and %s_race.Map = %s_maps.Map) order by RAND() limit 1;", pSqlServer->GetPrefix(), g_Config.m_SvServerType, pData->m_Num, pSqlServer->GetPrefix(), pData->m_Name.ClrStr(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		else
			str_format(aBuf, sizeof(aBuf), "select * from %s_maps where Server = \"%s\" and not exists (select * from %s_race where Name = \"%s\" and %s_race.Map = %s_maps.Map) order by RAND() limit 1;", pSqlServer->GetPrefix(), g_Config.m_SvServerType, pSqlServer->GetPrefix(), pData->m_Name.ClrStr(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		pSqlServer->executeSqlQuery(aBuf);

		if(pSqlServer->GetResults()->rowsCount() != 1)
		{
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "You have no unfinished maps on this server!");
		}
		else
		{
			pSqlServer->GetResults()->next();
			char aMap[128];
			strcpy(aMap, pSqlServer->GetResults()->getString("Map").c_str());

			str_format(aBuf, sizeof(aBuf), "change_map \"%s\"", aMap);
			pData->GameServer()->Console()->ExecuteLine(aBuf);
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
}

void CSqlScore::SaveTeam(int Team, const char* Code, int ClientID, const char* Server)
{
	if((g_Config.m_SvTeam == 3 || (Team > 0 && Team < MAX_CLIENTS)) && ((CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.Count(Team) > 0)
	{
		if(((CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.GetSaving(Team))
			return;
		((CGameControllerDDRace*)(GameServer()->m_pController))->m_Teams.SetSaving(Team, true);
	}
	else
	{
		GameServer()->SendChatTarget(ClientID, "You have to be in a Team (from 1-63)");
		return;
	}

	CSqlTeamSave *Tmp = new CSqlTeamSave();
	Tmp->m_Team = Team;
	Tmp->m_ClientID = ClientID;
	Tmp->m_Code = Code;
	str_copy(Tmp->m_Server, Server, sizeof(Tmp->m_Server));

	void *SaveThread = thread_init(ExecSqlFunc, new CSqlExecData(SaveTeamThread, Tmp, false));
	thread_detach(SaveThread);
}

bool CSqlScore::SaveTeamThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlTeamSave *pData = dynamic_cast<const CSqlTeamSave *>(pGameData);

	try
	{
		int Team = pData->m_Team;

		if (HandleFailure)
			return true;

		char TeamString[65536];

		int Num = -1;

		if((g_Config.m_SvTeam == 3 || (Team > 0 && Team < MAX_CLIENTS)) && ((CGameControllerDDRace*)(pData->GameServer()->m_pController))->m_Teams.Count(Team) > 0)
		{
			CSaveTeam SavedTeam(pData->GameServer()->m_pController);
			Num = SavedTeam.save(Team);
			switch (Num)
			{
				case 1:
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "You have to be in a Team (from 1-63)");
					break;
				case 2:
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "Could not find your Team");
					break;
				case 3:
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "Unable to find all Characters");
					break;
				case 4:
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "Your team is not started yet");
					break;
			}
			if(!Num)
			{
				str_copy(TeamString, SavedTeam.GetString(), sizeof(TeamString));
				sqlstr::ClearString(TeamString, sizeof(TeamString));
			}
		}
		else
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "You have to be in a Team (from 1-63)");

		if (Num)
			return true;

		try
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "select Savegame from %s_saves where Code = '%s' and Map = '%s';", pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
			pSqlServer->executeSqlQuery(aBuf);

			if (pSqlServer->GetResults()->rowsCount() == 0)
			{
				char aBuf[65536];
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_saves(Savegame, Map, Code, Timestamp, Server) VALUES ('%s', '%s', '%s', CURRENT_TIMESTAMP(), '%s')", pSqlServer->GetPrefix(), TeamString, pData->m_Map.ClrStr(), pData->m_Code.ClrStr(), pData->m_Server);
				dbg_msg("sql", "%s", aBuf);
				pSqlServer->executeSql(aBuf);

				// be sure to keep all calls to pData->GameServer() after inserting the save, otherwise it might be lost due to CGameContextError.

				char aBuf2[256];
				str_format(aBuf2, sizeof(aBuf2), "Team successfully saved. Use '/load %s' to continue", pData->m_Code.Str());
				pData->GameServer()->SendChatTeam(Team, aBuf2);
				((CGameControllerDDRace*)(pData->GameServer()->m_pController))->m_Teams.KillSavedTeam(Team);
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

	return true;
}

void CSqlScore::LoadTeam(const char* Code, int ClientID)
{
	CSqlTeamLoad *Tmp = new CSqlTeamLoad();
	Tmp->m_Code = Code;
	Tmp->m_ClientID = ClientID;

	void *LoadThread = thread_init(ExecSqlFunc, new CSqlExecData(LoadTeamThread, Tmp));
	thread_detach(LoadThread);
}

void CSqlScore::LoadTeamInfo(const char* Code, int ClientID)
{
	CSqlTeamLoad *Tmp = new CSqlTeamLoad();
	Tmp->m_Code = Code;
	Tmp->m_ClientID = ClientID;

	void *LoadThread = thread_init(ExecSqlFunc, new CSqlExecData(LoadTeamInfoThread, Tmp));
	thread_detach(LoadThread);
}

bool CSqlScore::LoadTeamInfoThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlTeamLoad *pData = dynamic_cast<const CSqlTeamLoad *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[768];
		str_format(aBuf, sizeof(aBuf), "select Savegame, Server, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago from %s_saves where Code = '%s' and Map = '%s';", pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);

		//TODO: what about thread safety?
		CPlayer *pPlayer = pData->GameServer()->m_apPlayers[pData->m_ClientID];
		CTeamLoadState *pTeamLoadState = &pPlayer->m_TeamLoadState;

		if (pPlayer && pTeamLoadState->m_State == CTeamLoadState::HOST_THREAD_INIT_LOAD)
		{
			if (pSqlServer->GetResults()->rowsCount() > 0)
			{
				pSqlServer->GetResults()->first();
				char ServerName[5];
				str_copy(ServerName, pSqlServer->GetResults()->getString("Server").c_str(), sizeof(ServerName));
				if(str_comp(ServerName, g_Config.m_SvSqlServerName))
				{
					str_format(aBuf, sizeof(aBuf), "You have to be on the '%s' server to load this savegame", ServerName);
					pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					pTeamLoadState->m_State = CTeamLoadState::HOST_THREAD_INIT_FAILED;
					goto end;
				}

				pSqlServer->GetResults()->getInt("Ago");
				int since = (int)pSqlServer->GetResults()->getInt("Ago");

				if(since < g_Config.m_SvSaveGamesDelay)
				{
					str_format(aBuf, sizeof(aBuf), "You have to wait %d seconds until you can load this savegame", g_Config.m_SvSaveGamesDelay - since);
					pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					pTeamLoadState->m_State = CTeamLoadState::HOST_THREAD_INIT_FAILED;
					goto end;
				}

				CSaveTeam *pSavedTeam = new CSaveTeam(pData->GameServer()->m_pController);

				int Num = pSavedTeam->LoadString(pSqlServer->GetResults()->getString("Savegame").c_str());

				if(Num)
				{
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "Unable to load savegame: data corrupted");
					pTeamLoadState->m_State = CTeamLoadState::HOST_THREAD_INIT_FAILED;
				}
				else
				{

					bool Found = false;
					for (int i = 0; i < pSavedTeam->GetMembersCount(); i++)
					{
						if(str_comp(pSavedTeam->SavedTees[i].GetName(), pData->Server()->ClientName(pData->m_ClientID)) == 0)
						{ Found = true; break; }
					}
					if (!Found)
					{
						pData->GameServer()->SendChatTarget(pData->m_ClientID, "You don't belong to this team");
						pTeamLoadState->m_State = CTeamLoadState::HOST_THREAD_INIT_FAILED;
					}
					else
					{
						// complete initiation by returning team savegame information to main thread
						pPlayer->m_TeamLoadState.m_pSaveTeam = pSavedTeam;
						pPlayer->m_TeamLoadState.m_State = CTeamLoadState::HOST_THREAD_INIT_DONE;
					}
				}
			}
			else
			{
				pData->GameServer()->SendChatTarget(pData->m_ClientID, "No such savegame for this map");
				pTeamLoadState->m_State = CTeamLoadState::HOST_THREAD_INIT_FAILED;
			}
		}

		end:
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
}

bool CSqlScore::LoadTeamThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure)
{
	const CSqlTeamLoad *pData = dynamic_cast<const CSqlTeamLoad *>(pGameData);

	if (HandleFailure)
		return true;

	try
	{
		char aBuf[768];
		str_format(aBuf, sizeof(aBuf), "select Savegame, Server, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago from %s_saves where Code = '%s' and Map = '%s';", pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
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
			int since = (int)pSqlServer->GetResults()->getInt("Ago");

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
					if(str_comp(SavedTeam.SavedTees[i].GetName(), pData->Server()->ClientName(pData->m_ClientID)) == 0)
					{ Found = true; break; }
				}
				if (!Found)
					pData->GameServer()->SendChatTarget(pData->m_ClientID, "You don't belong to this team");
				else
				{

					int n;
					for(n = 1; n<64; n++)
					{
						if(((CGameControllerDDRace*)(pData->GameServer()->m_pController))->m_Teams.Count(n) == 0)
							break;
					}

					if(((CGameControllerDDRace*)(pData->GameServer()->m_pController))->m_Teams.Count(n) > 0)
					{
						n = ((CGameControllerDDRace*)(pData->GameServer()->m_pController))->m_Teams.m_Core.Team(pData->m_ClientID); // if all Teams are full your the only one in your team
					}

					Num = SavedTeam.load(n);

					if(Num == 1)
					{
						pData->GameServer()->SendChatTarget(pData->m_ClientID, "You have to be in a team (from 1-63)");
					}
					else if(Num >= 10 && Num < 100)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "Unable to find player: '%s'", SavedTeam.SavedTees[Num-10].GetName());
						pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else if(Num >= 100)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "%s is racing right now, Team can't be loaded if a Tee is racing already", SavedTeam.SavedTees[Num-100].GetName());
						pData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else
					{
						pData->GameServer()->SendChatTeam(n, "Loading successfully done");
						char aBuf[512];
						str_format(aBuf, sizeof(aBuf), "DELETE from %s_saves where Code='%s' and Map='%s';", pSqlServer->GetPrefix(), pData->m_Code.ClrStr(), pData->m_Map.ClrStr());
						pSqlServer->executeSql(aBuf);
					}
				}
			}
		}
		else
			pData->GameServer()->SendChatTarget(pData->m_ClientID, "No such savegame for this map");

		end:
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
}

#endif
