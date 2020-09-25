#include "score.h"
#include "entities/character.h"
#include "gamemodes/DDRace.h"
#include "save.h"

#include <base/system.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/sql_string_helpers.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <random>

CScorePlayerResult::CScorePlayerResult() :
	m_Done(false)
{
	SetVariant(Variant::DIRECT);
}

void CScorePlayerResult::SetVariant(Variant v)
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

CTeamrank::CTeamrank() :
	m_NumNames(0)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aaNames[i][0] = '\0';
	mem_zero(&m_TeamID.m_aData, sizeof(m_TeamID));
}

bool CTeamrank::NextSqlResult(IDbConnection *pSqlServer)
{
	pSqlServer->GetBlob(1, m_TeamID.m_aData, sizeof(m_TeamID.m_aData));
	pSqlServer->GetString(2, m_aaNames[0], sizeof(m_aaNames[0]));
	m_NumNames = 1;
	while(pSqlServer->Step())
	{
		CUuid TeamID;
		pSqlServer->GetBlob(1, TeamID.m_aData, sizeof(TeamID.m_aData));
		if(m_TeamID != TeamID)
			return true;
		pSqlServer->GetString(2, m_aaNames[m_NumNames], sizeof(m_aaNames[m_NumNames]));
		m_NumNames++;
	}
	return false;
}

bool CTeamrank::SamePlayers(const std::vector<std::string> *aSortedNames)
{
	if(aSortedNames->size() != m_NumNames)
		return false;
	for(unsigned int i = 0; i < m_NumNames; i++)
	{
		if(str_comp(aSortedNames->at(i).c_str(), m_aaNames[i]) != 0)
			return false;
	}
	return true;
}

std::shared_ptr<CScorePlayerResult> CScore::NewSqlPlayerResult(int ClientID)
{
	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientID];
	if(pCurPlayer->m_ScoreQueryResult != nullptr) // TODO: send player a message: "too many requests"
		return nullptr;
	pCurPlayer->m_ScoreQueryResult = std::make_shared<CScorePlayerResult>();
	return pCurPlayer->m_ScoreQueryResult;
}

void CScore::ExecPlayerThread(
	bool (*pFuncPtr)(IDbConnection *, const ISqlData *),
	const char *pThreadName,
	int ClientID,
	const char *pName,
	int Offset)
{
	auto pResult = NewSqlPlayerResult(ClientID);
	if(pResult == nullptr)
		return;
	auto Tmp = std::unique_ptr<CSqlPlayerRequest>(new CSqlPlayerRequest(pResult));
	str_copy(Tmp->m_Name, pName, sizeof(Tmp->m_Name));
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));
	str_copy(Tmp->m_RequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_RequestingPlayer));
	Tmp->m_Offset = Offset;

	m_pPool->Execute(pFuncPtr, std::move(Tmp), pThreadName);
}

bool CScore::RateLimitPlayer(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(pPlayer == 0)
		return true;
	if(pPlayer->m_LastSQLQuery + g_Config.m_SvSqlQueriesDelay * Server()->TickSpeed() >= Server()->Tick())
		return true;
	pPlayer->m_LastSQLQuery = Server()->Tick();
	return false;
}

void CScore::GeneratePassphrase(char *pBuf, int BufSize)
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

CScore::CScore(CGameContext *pGameServer, CDbConnectionPool *pPool) :
	m_pPool(pPool),
	m_pGameServer(pGameServer),
	m_pServer(pGameServer->Server())
{
	auto InitResult = std::make_shared<CScoreInitResult>();
	auto Tmp = std::unique_ptr<CSqlInitData>(new CSqlInitData(InitResult));
	((CGameControllerDDRace *)(pGameServer->m_pController))->m_pInitResult = InitResult;
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));

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
	m_pPool->Execute(Init, std::move(Tmp), "load best time");
}

bool CScore::Init(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlInitData *pData = dynamic_cast<const CSqlInitData *>(pGameData);

	char aBuf[512];
	// get the best time
	str_format(aBuf, sizeof(aBuf),
		"SELECT Time FROM %s_race WHERE Map=? ORDER BY `Time` ASC LIMIT 1;",
		pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);

	if(pSqlServer->Step())
		pData->m_pResult->m_CurrentRecord = pSqlServer->GetFloat(1);

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::LoadPlayerData(int ClientID)
{
	ExecPlayerThread(LoadPlayerDataThread, "load player data", ClientID, "", 0);
}

// update stuff
bool CScore::LoadPlayerDataThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	pData->m_pResult->SetVariant(CScorePlayerResult::PLAYER_INFO);

	char aBuf[512];
	// get best race time
	str_format(aBuf, sizeof(aBuf),
		"SELECT Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, "
		"  cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, "
		"  cp21, cp22, cp23, cp24, cp25 "
		"FROM %s_race "
		"WHERE Map = ? AND Name = ? "
		"ORDER BY Time ASC "
		"LIMIT 1;",
		pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, pData->m_RequestingPlayer);

	if(pSqlServer->Step())
	{
		// get the best time
		float Time = pSqlServer->GetFloat(1);
		pData->m_pResult->m_Data.m_Info.m_Time = Time;
		pData->m_pResult->m_Data.m_Info.m_Score = -Time;
		pData->m_pResult->m_Data.m_Info.m_HasFinishScore = true;

		if(g_Config.m_SvCheckpointSave)
		{
			for(int i = 0; i < NUM_CHECKPOINTS; i++)
			{
				pData->m_pResult->m_Data.m_Info.m_CpTime[i] = pSqlServer->GetFloat(i + 2);
			}
		}
	}

	// birthday check
	str_format(aBuf, sizeof(aBuf),
		"SELECT CURRENT_TIMESTAMP AS Current, MIN(Timestamp) AS Stamp "
		"FROM %s_race "
		"WHERE Name = ?",
		pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_RequestingPlayer);

	if(pSqlServer->Step() && !pSqlServer->IsNull(2))
	{
		char aCurrent[TIMESTAMP_STR_LENGTH];
		pSqlServer->GetString(1, aCurrent, sizeof(aCurrent));
		char aStamp[TIMESTAMP_STR_LENGTH];
		pSqlServer->GetString(2, aStamp, sizeof(aStamp));
		int CurrentYear, CurrentMonth, CurrentDay;
		int StampYear, StampMonth, StampDay;
		if(sscanf(aCurrent, "%d-%d-%d", &CurrentYear, &CurrentMonth, &CurrentDay) == 3 && sscanf(aStamp, "%d-%d-%d", &StampYear, &StampMonth, &StampDay) == 3 && CurrentMonth == StampMonth && CurrentDay == StampDay)
			pData->m_pResult->m_Data.m_Info.m_Birthday = CurrentYear - StampYear;
	}
	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::MapVote(int ClientID, const char *MapName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(MapVoteThread, "map vote", ClientID, MapName, 0);
}

bool CScore::MapVoteThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	char aFuzzyMap[128];
	str_copy(aFuzzyMap, pData->m_Name, sizeof(aFuzzyMap));
	sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

	char aMapPrefix[128];
	str_copy(aMapPrefix, pData->m_Name, sizeof(aMapPrefix));
	str_append(aMapPrefix, "%", sizeof(aMapPrefix));

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Map, Server "
		"FROM %s_maps "
		"WHERE Map LIKE %s "
		"ORDER BY "
		"  CASE WHEN Map = ? THEN 0 ELSE 1 END, "
		"  CASE WHEN Map LIKE ? THEN 0 ELSE 1 END, "
		"  LENGTH(Map), Map "
		"LIMIT 1;",
		pSqlServer->GetPrefix(), pSqlServer->CollateNocase());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, aFuzzyMap);
	pSqlServer->BindString(2, pData->m_Name);
	pSqlServer->BindString(3, aMapPrefix);

	if(pSqlServer->Step())
	{
		pData->m_pResult->SetVariant(CScorePlayerResult::MAP_VOTE);
		auto MapVote = &pData->m_pResult->m_Data.m_MapVote;
		pSqlServer->GetString(1, MapVote->m_Map, sizeof(MapVote->m_Map));
		pSqlServer->GetString(2, MapVote->m_Server, sizeof(MapVote->m_Server));
		strcpy(MapVote->m_Reason, "/map");

		for(char *p = MapVote->m_Server; *p; p++) // lower case server
			*p = tolower(*p);
	}
	else
	{
		pData->m_pResult->SetVariant(CScorePlayerResult::DIRECT);
		str_format(paMessages[0], sizeof(paMessages[0]),
			"No map like \"%s\" found. "
			"Try adding a '%%' at the start if you don't know the first character. "
			"Example: /map %%castle for \"Out of Castle\"",
			pData->m_Name);
	}
	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::MapInfo(int ClientID, const char *MapName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(MapInfoThread, "map info", ClientID, MapName, 0);
}

bool CScore::MapInfoThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);

	char aFuzzyMap[128];
	str_copy(aFuzzyMap, pData->m_Name, sizeof(aFuzzyMap));
	sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

	char aMapPrefix[128];
	str_copy(aMapPrefix, pData->m_Name, sizeof(aMapPrefix));
	str_append(aMapPrefix, "%", sizeof(aMapPrefix));

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("l.Timestamp", aTimestamp, sizeof(aTimestamp));

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf),
		"SELECT l.Map, l.Server, Mapper, Points, Stars, "
		"  (SELECT COUNT(Name) FROM %s_race WHERE Map = l.Map) AS Finishes, "
		"  (SELECT COUNT(DISTINCT Name) FROM %s_race WHERE Map = l.Map) AS Finishers, "
		"  (SELECT ROUND(AVG(Time)) FROM %s_race WHERE Map = l.Map) AS Average, "
		"  %s AS Stamp, "
		"  %s-%s AS Ago, "
		"  (SELECT MIN(Time) FROM %s_race WHERE Map = l.Map AND Name = ?) AS OwnTime "
		"FROM ("
		"  SELECT * FROM %s_maps "
		"  WHERE Map LIKE %s "
		"  ORDER BY "
		"    CASE WHEN Map = ? THEN 0 ELSE 1 END, "
		"    CASE WHEN Map LIKE ? THEN 0 ELSE 1 END, "
		"    LENGTH(Map), "
		"    Map "
		"  LIMIT 1"
		") as l;",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
		aTimestamp, aCurrentTimestamp, aTimestamp,
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
		pSqlServer->CollateNocase());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_RequestingPlayer);
	pSqlServer->BindString(2, aFuzzyMap);
	pSqlServer->BindString(3, pData->m_Name);
	pSqlServer->BindString(4, aMapPrefix);

	if(pSqlServer->Step())
	{
		char aMap[MAX_MAP_LENGTH];
		pSqlServer->GetString(1, aMap, sizeof(aMap));
		char aServer[32];
		pSqlServer->GetString(2, aServer, sizeof(aServer));
		char aMapper[128];
		pSqlServer->GetString(3, aMapper, sizeof(aMapper));
		int Points = pSqlServer->GetInt(4);
		int Stars = pSqlServer->GetInt(5);
		int Finishes = pSqlServer->GetInt(6);
		int Finishers = pSqlServer->GetInt(7);
		int Average = pSqlServer->GetInt(8);
		int Stamp = pSqlServer->GetInt(9);
		int Ago = pSqlServer->GetInt(10);
		float OwnTime = pSqlServer->GetFloat(11);

		char aAgoString[40] = "\0";
		char aReleasedString[60] = "\0";
		if(Stamp != 0)
		{
			sqlstr::AgoTimeToString(Ago, aAgoString);
			str_format(aReleasedString, sizeof(aReleasedString), ", released %s ago", aAgoString);
		}

		char aAverageString[60] = "\0";
		if(Average > 0)
		{
			str_format(aAverageString, sizeof(aAverageString), " in %d:%02d average", Average / 60, Average % 60);
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
		if(OwnTime > 0)
		{
			str_format(aOwnFinishesString, sizeof(aOwnFinishesString),
				", your time: %02d:%05.2f", (int)(OwnTime / 60), OwnTime - ((int)OwnTime / 60 * 60));
		}

		str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
			"\"%s\" by %s on %s, %s, %d %s%s, %d %s by %d %s%s%s",
			aMap, aMapper, aServer, aStars,
			Points, Points == 1 ? "point" : "points",
			aReleasedString,
			Finishes, Finishes == 1 ? "finish" : "finishes",
			Finishers, Finishers == 1 ? "tee" : "tees",
			aAverageString, aOwnFinishesString);
	}
	else
	{
		str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
			"No map like \"%s\" found.", pData->m_Name);
	}
	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::SaveScore(int ClientID, float Time, const char *pTimestamp, float CpTime[NUM_CHECKPOINTS], bool NotEligible)
{
	CConsole *pCon = (CConsole *)GameServer()->Console();
	if(pCon->m_Cheated || NotEligible)
		return;

	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientID];
	if(pCurPlayer->m_ScoreFinishResult != nullptr)
		dbg_msg("sql", "WARNING: previous save score result didn't complete, overwriting it now");
	pCurPlayer->m_ScoreFinishResult = std::make_shared<CScorePlayerResult>();
	auto Tmp = std::unique_ptr<CSqlScoreData>(new CSqlScoreData(pCurPlayer->m_ScoreFinishResult));
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_GameUuid, sizeof(Tmp->m_GameUuid));
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_Name, Server()->ClientName(ClientID), sizeof(Tmp->m_Name));
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCpCurrent[i] = CpTime[i];

	m_pPool->ExecuteWrite(SaveScoreThread, std::move(Tmp), "save score");
}

bool CScore::SaveScoreThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure)
{
	const CSqlScoreData *pData = dynamic_cast<const CSqlScoreData *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	char aBuf[1024];

	str_format(aBuf, sizeof(aBuf),
		"SELECT COUNT(*) AS NumFinished FROM %s_race WHERE Map=? AND Name=? ORDER BY time ASC LIMIT 1;",
		pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, pData->m_Name);

	pSqlServer->Step();
	int NumFinished = pSqlServer->GetInt(1);
	if(NumFinished == 0)
	{
		str_format(aBuf, sizeof(aBuf), "SELECT Points FROM %s_maps WHERE Map=?", pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_Map);

		if(pSqlServer->Step())
		{
			int Points = pSqlServer->GetInt(1);
			pSqlServer->AddPoints(pData->m_Name, Points);
			str_format(paMessages[0], sizeof(paMessages[0]),
				"You earned %d point%s for finishing this map!",
				Points, Points == 1 ? "" : "s");
		}
	}

	// save score. Can't fail, because no UNIQUE/PRIMARY KEY constrain is defined.
	str_format(aBuf, sizeof(aBuf),
		"%s INTO %s_race("
		"	Map, Name, Timestamp, Time, Server, "
		"	cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, "
		"	cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, "
		"	GameID, DDNet7) "
		"VALUES (?, ?, %s, %.2f, ?, "
		"	%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, "
		"	%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, "
		"	%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, "
		"	?, false);",
		pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
		pSqlServer->InsertTimestampAsUtc(), pData->m_Time,
		pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2],
		pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5],
		pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8],
		pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11],
		pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14],
		pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17],
		pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20],
		pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23],
		pData->m_aCpCurrent[24]);
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, pData->m_Name);
	pSqlServer->BindString(3, pData->m_aTimestamp);
	pSqlServer->BindString(4, g_Config.m_SvSqlServerName);
	pSqlServer->BindString(5, pData->m_GameUuid);
	pSqlServer->Print();
	pSqlServer->Step();

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::SaveTeamScore(int *aClientIDs, unsigned int Size, float Time, const char *pTimestamp)
{
	CConsole *pCon = (CConsole *)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	for(unsigned int i = 0; i < Size; i++)
	{
		if(GameServer()->m_apPlayers[aClientIDs[i]]->m_NotEligibleForFinish)
			return;
	}
	auto Tmp = std::unique_ptr<CSqlTeamScoreData>(new CSqlTeamScoreData());
	for(unsigned int i = 0; i < Size; i++)
		str_copy(Tmp->m_aNames[i], Server()->ClientName(aClientIDs[i]), sizeof(Tmp->m_aNames[i]));
	Tmp->m_Size = Size;
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_GameUuid, sizeof(Tmp->m_GameUuid));
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));

	m_pPool->ExecuteWrite(SaveTeamScoreThread, std::move(Tmp), "save team score");
}

bool CScore::SaveTeamScoreThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure)
{
	const CSqlTeamScoreData *pData = dynamic_cast<const CSqlTeamScoreData *>(pGameData);

	char aBuf[512];

	// get the names sorted in a tab separated string
	std::vector<std::string> aNames;
	for(unsigned int i = 0; i < pData->m_Size; i++)
		aNames.push_back(pData->m_aNames[i]);

	std::sort(aNames.begin(), aNames.end());
	str_format(aBuf, sizeof(aBuf),
		"SELECT l.ID, Name, Time "
		"FROM (" // preselect teams with first name in team
		"  SELECT ID "
		"  FROM %s_teamrace "
		"  WHERE Map = ? AND Name = ? AND DDNet7 = false"
		") as l INNER JOIN %s_teamrace AS r ON l.ID = r.ID "
		"ORDER BY l.ID, Name COLLATE %s;",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->BinaryCollate());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, pData->m_aNames[0]);

	bool FoundTeam = false;
	float Time;
	CTeamrank Teamrank;
	if(pSqlServer->Step())
	{
		bool SearchTeam = true;
		while(SearchTeam)
		{
			Time = pSqlServer->GetFloat(3);
			SearchTeam = Teamrank.NextSqlResult(pSqlServer);
			if(Teamrank.SamePlayers(&aNames))
			{
				FoundTeam = true;
				break;
			}
		}
	}
	if(FoundTeam)
	{
		dbg_msg("sql", "found team rank from same team (old time: %f, new time: %f)", Time, pData->m_Time);
		if(pData->m_Time < Time)
		{
			str_format(aBuf, sizeof(aBuf),
				"UPDATE %s_teamrace SET Time=%.2f, Timestamp=?, DDNet7=false, GameID=? WHERE ID = ?;",
				pSqlServer->GetPrefix(), pData->m_Time);
			pSqlServer->PrepareStatement(aBuf);
			pSqlServer->BindString(1, pData->m_aTimestamp);
			pSqlServer->BindString(2, pData->m_GameUuid);
			pSqlServer->BindBlob(3, Teamrank.m_TeamID.m_aData, sizeof(Teamrank.m_TeamID.m_aData));
			pSqlServer->Print();
			pSqlServer->Step();
		}
	}
	else
	{
		CUuid GameID = RandomUuid();
		for(unsigned int i = 0; i < pData->m_Size; i++)
		{
			// if no entry found... create a new one
			str_format(aBuf, sizeof(aBuf),
				"%s INTO %s_teamrace(Map, Name, Timestamp, Time, ID, GameID, DDNet7) "
				"VALUES (?, ?, %s, %.2f, ?, ?, false);",
				pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
				pSqlServer->InsertTimestampAsUtc(), pData->m_Time);
			pSqlServer->PrepareStatement(aBuf);
			pSqlServer->BindString(1, pData->m_Map);
			pSqlServer->BindString(2, pData->m_aNames[i]);
			pSqlServer->BindString(3, pData->m_aTimestamp);
			pSqlServer->BindBlob(4, GameID.m_aData, sizeof(GameID.m_aData));
			pSqlServer->BindString(5, pData->m_GameUuid);
			pSqlServer->Print();
			pSqlServer->Step();
		}
	}
	return true;
}

void CScore::ShowRank(int ClientID, const char *pName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowRankThread, "show rank", ClientID, pName, 0);
}

bool CScore::ShowRankThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);

	// check sort method
	char aBuf[600];

	str_format(aBuf, sizeof(aBuf),
		"SELECT Rank, Name, Time "
		"FROM ("
		"  SELECT RANK() OVER w AS Rank, Name, MIN(Time) AS Time "
		"  FROM %s_race "
		"  WHERE Map = ? "
		"  GROUP BY Name "
		"  WINDOW w AS (ORDER BY Time)"
		") as a "
		"WHERE Name = ?;",
		pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, pData->m_Name);

	if(pSqlServer->Step())
	{
		int Rank = pSqlServer->GetInt(1);
		float Time = pSqlServer->GetFloat(3);
		if(g_Config.m_SvHideScore)
		{
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
				"Your time: %02d:%05.2f", (int)(Time / 60), Time - ((int)Time / 60 * 60));
		}
		else
		{
			char aName[MAX_NAME_LENGTH];
			pSqlServer->GetString(2, aName, sizeof(aName));
			pData->m_pResult->m_MessageKind = CScorePlayerResult::ALL;
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
				"%d. %s Time: %02d:%05.2f, requested by %s",
				Rank, aName, (int)(Time / 60), Time - ((int)Time / 60 * 60), pData->m_RequestingPlayer);
		}
	}
	else
	{
		str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
			"%s is not ranked", pData->m_Name);
	}

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::ShowTeamRank(int ClientID, const char *pName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowTeamRankThread, "show team rank", ClientID, pName, 0);
}

bool CScore::ShowTeamRankThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);

	// check sort method
	char aBuf[2400];

	str_format(aBuf, sizeof(aBuf),
		"SELECT l.ID, Name, Time, Rank "
		"FROM (" // teamrank score board
		"  SELECT RANK() OVER w AS Rank, ID "
		"  FROM %s_teamrace "
		"  WHERE Map = ? "
		"  GROUP BY ID "
		"  WINDOW w AS (ORDER BY Time)"
		") AS TeamRank INNER JOIN (" // select rank with Name in team
		"  SELECT ID "
		"  FROM %s_teamrace "
		"  WHERE Map = ? AND Name = ? "
		"  ORDER BY Time "
		"  LIMIT 1"
		") AS l ON TeamRank.ID = l.ID "
		"INNER JOIN %s_teamrace AS r ON l.ID = r.ID ",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, pData->m_Map);
	pSqlServer->BindString(3, pData->m_Name);

	if(pSqlServer->Step())
	{
		float Time = pSqlServer->GetFloat(3);
		int Rank = pSqlServer->GetInt(4);
		CTeamrank Teamrank;
		Teamrank.NextSqlResult(pSqlServer);

		char aFormattedNames[512] = "";
		for(unsigned int Name = 0; Name < Teamrank.m_NumNames; Name++)
		{
			str_append(aFormattedNames, Teamrank.m_aaNames[Name], sizeof(aFormattedNames));

			if(Name < Teamrank.m_NumNames - 2)
				str_append(aFormattedNames, ", ", sizeof(aFormattedNames));
			else if(Name < Teamrank.m_NumNames - 1)
				str_append(aFormattedNames, " & ", sizeof(aFormattedNames));
		}

		if(g_Config.m_SvHideScore)
		{
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
				"Your team time: %02d:%05.02f", (int)(Time / 60), Time - ((int)Time / 60 * 60));
		}
		else
		{
			pData->m_pResult->m_MessageKind = CScorePlayerResult::ALL;
			str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
				"%d. %s Team time: %02d:%05.02f, requested by %s",
				Rank, aFormattedNames, (int)(Time / 60), Time - ((int)Time / 60 * 60), pData->m_RequestingPlayer);
		}
	}
	else
	{
		str_format(pData->m_pResult->m_Data.m_aaMessages[0], sizeof(pData->m_pResult->m_Data.m_aaMessages[0]),
			"%s has no team ranks", pData->m_Name);
	}
	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::ShowTop5(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowTop5Thread, "show top5", ClientID, "", Offset);
}

bool CScore::ShowTop5Thread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);

	int LimitStart = maximum(abs(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	// check sort method
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Name, Time, Rank "
		"FROM ("
		"  SELECT RANK() OVER w AS Rank, Name, MIN(Time) AS Time "
		"  FROM %s_race "
		"  WHERE Map = ? "
		"  GROUP BY Name "
		"  WINDOW w AS (ORDER BY Time)"
		") as a "
		"ORDER BY Rank %s "
		"LIMIT %d, 5;",
		pSqlServer->GetPrefix(),
		pOrder,
		LimitStart);
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);

	// show top5
	strcpy(pData->m_pResult->m_Data.m_aaMessages[0], "----------- Top 5 -----------");

	int Line = 1;
	while(pSqlServer->Step())
	{
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(1, aName, sizeof(aName));
		float Time = pSqlServer->GetFloat(2);
		int Rank = pSqlServer->GetInt(3);
		str_format(pData->m_pResult->m_Data.m_aaMessages[Line], sizeof(pData->m_pResult->m_Data.m_aaMessages[Line]),
			"%d. %s Time: %02d:%05.2f",
			Rank, aName, (int)(Time / 60), Time - ((int)Time / 60 * 60));
		Line++;
	}
	strcpy(pData->m_pResult->m_Data.m_aaMessages[Line], "-------------------------------");

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::ShowTeamTop5(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowTeamTop5Thread, "show team top5", ClientID, "", Offset);
}

bool CScore::ShowTeamTop5Thread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(abs(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	// check sort method
	char aBuf[512];

	str_format(aBuf, sizeof(aBuf),
		"SELECT Name, Time, Rank, TeamSize "
		"FROM (" // limit to 5
		"  SELECT TeamSize, Rank, ID "
		"  FROM (" // teamrank score board
		"    SELECT RANK() OVER w AS Rank, ID, COUNT(*) AS Teamsize "
		"    FROM %s_teamrace "
		"    WHERE Map = ? "
		"    GROUP BY Id "
		"    WINDOW w AS (ORDER BY Time)"
		"  ) as l1 "
		"  ORDER BY Rank %s "
		"  LIMIT %d, 5"
		") as l2 "
		"INNER JOIN %s_teamrace as r ON l2.ID = r.ID "
		"ORDER BY Rank %s, r.ID, Name ASC;",
		pSqlServer->GetPrefix(), pOrder, LimitStart, pSqlServer->GetPrefix(), pOrder);
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);

	// show teamtop5
	int Line = 0;
	strcpy(paMessages[Line++], "------- Team Top 5 -------");

	if(pSqlServer->Step())
	{
		for(Line = 1; Line < 6; Line++) // print
		{
			bool Last = false;
			float Time = pSqlServer->GetFloat(2);
			int Rank = pSqlServer->GetInt(3);
			int TeamSize = pSqlServer->GetInt(4);

			char aNames[2300] = {0};
			for(int i = 0; i < TeamSize; i++)
			{
				char aName[MAX_NAME_LENGTH];
				pSqlServer->GetString(1, aName, sizeof(aName));
				str_append(aNames, aName, sizeof(aNames));
				if(i < TeamSize - 2)
					str_append(aNames, ", ", sizeof(aNames));
				else if(i == TeamSize - 2)
					str_append(aNames, " & ", sizeof(aNames));
				if(!pSqlServer->Step())
				{
					Last = true;
					break;
				}
			}
			str_format(paMessages[Line], sizeof(paMessages[Line]), "%d. %s Team Time: %02d:%05.2f",
				Rank, aNames, (int)(Time / 60), Time - ((int)Time / 60 * 60));
			if(Last)
			{
				Line++;
				break;
			}
		}
	}

	strcpy(paMessages[Line], "-------------------------------");
	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::ShowTimes(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowTimesThread, "show times", ClientID, "", Offset);
}

void CScore::ShowTimes(int ClientID, const char *pName, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowTimesThread, "show times", ClientID, pName, Offset);
}

bool CScore::ShowTimesThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(abs(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "DESC" : "ASC";

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("Timestamp", aTimestamp, sizeof(aTimestamp));
	char aBuf[512];
	if(pData->m_Name[0] != '\0') // last 5 times of a player
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Time, (%s-%s) as Ago, %s as Stamp "
			"FROM %s_race "
			"WHERE Map = ? AND Name = ? "
			"ORDER BY Timestamp %s "
			"LIMIT ?, 5;",
			aCurrentTimestamp, aTimestamp, aTimestamp,
			pSqlServer->GetPrefix(), pOrder);
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_Map);
		pSqlServer->BindString(2, pData->m_Name);
		pSqlServer->BindInt(3, LimitStart);
	}
	else // last 5 times of server
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Time, (%s-%s) as Ago, %s as Stamp, Name "
			"FROM %s_race "
			"WHERE Map = ? "
			"ORDER BY Timestamp %s "
			"LIMIT ?, 5;",
			aCurrentTimestamp, aTimestamp, aTimestamp,
			pSqlServer->GetPrefix(), pOrder);
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_Map);
		pSqlServer->BindInt(2, LimitStart);
	}

	// show top5
	if(!pSqlServer->Step())
	{
		strcpy(paMessages[0], "There are no times in the specified range");
		pData->m_pResult->m_Done = true;
		return true;
	}

	strcpy(paMessages[0], "------------- Last Times -------------");
	int Line = 1;

	do
	{
		float Time = pSqlServer->GetFloat(1);
		int Ago = pSqlServer->GetInt(2);
		int Stamp = pSqlServer->GetInt(3);

		char aAgoString[40] = "\0";
		sqlstr::AgoTimeToString(Ago, aAgoString);

		if(pData->m_Name[0] != '\0') // last 5 times of a player
		{
			if(Stamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%02d:%05.02f, don't know how long ago",
					(int)(Time / 60), Time - ((int)Time / 60 * 60));
			else
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s ago, %02d:%05.02f",
					aAgoString, (int)(Time / 60), Time - ((int)Time / 60 * 60));
		}
		else // last 5 times of the server
		{
			char aName[MAX_NAME_LENGTH];
			pSqlServer->GetString(4, aName, sizeof(aName));
			if(Stamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
			{
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s, %02d:%05.02f, don't know when",
					aName, (int)(Time / 60), Time - ((int)Time / 60 * 60));
			}
			else
			{
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s, %s ago, %02d:%05.02f",
					aName, aAgoString, (int)(Time / 60), Time - ((int)Time / 60 * 60));
			}
		}
		Line++;
	} while(pSqlServer->Step());
	strcpy(paMessages[Line], "----------------------------------------------------");

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::ShowPoints(int ClientID, const char *pName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowPointsThread, "show points", ClientID, pName, 0);
}

bool CScore::ShowPointsThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT ("
		"  SELECT COUNT(Name) + 1 FROM %s_points WHERE Points > ("
		"    SELECT points FROM %s_points WHERE Name = ?"
		")) as Rank, Points, Name "
		"FROM %s_points WHERE Name = ?;",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Name);
	pSqlServer->BindString(2, pData->m_Name);

	if(pSqlServer->Step())
	{
		int Rank = pSqlServer->GetInt(1);
		int Count = pSqlServer->GetInt(2);
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(3, aName, sizeof(aName));
		pData->m_pResult->m_MessageKind = CScorePlayerResult::ALL;
		str_format(paMessages[0], sizeof(paMessages[0]),
			"%d. %s Points: %d, requested by %s",
			Rank, aName, Count, pData->m_RequestingPlayer);
	}
	else
	{
		str_format(paMessages[0], sizeof(paMessages[0]),
			"%s has not collected any points so far", pData->m_Name);
	}

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::ShowTopPoints(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(ShowTopPointsThread, "show top points", ClientID, "", Offset);
}

bool CScore::ShowTopPointsThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(abs(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Rank, Points, Name "
		"FROM ("
		"  SELECT RANK() OVER w AS Rank, Points, Name "
		"  FROM %s_points "
		"  WINDOW w as (ORDER BY Points DESC)"
		") as a "
		"ORDER BY Rank %s "
		"LIMIT ?, 5;",
		pSqlServer->GetPrefix(), pOrder);
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindInt(1, LimitStart);

	// show top points
	strcpy(paMessages[0], "-------- Top Points --------");

	int Line = 1;
	while(pSqlServer->Step())
	{
		int Rank = pSqlServer->GetInt(1);
		int Points = pSqlServer->GetInt(2);
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(3, aName, sizeof(aName));
		str_format(paMessages[Line], sizeof(paMessages[Line]),
			"%d. %s Points: %d", Rank, aName, Points);
		Line++;
	}
	strcpy(paMessages[Line], "-------------------------------");

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::RandomMap(int ClientID, int Stars)
{
	auto pResult = std::make_shared<CScoreRandomMapResult>(ClientID);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto Tmp = std::unique_ptr<CSqlRandomMapRequest>(new CSqlRandomMapRequest(pResult));
	Tmp->m_Stars = Stars;
	str_copy(Tmp->m_CurrentMap, g_Config.m_SvMap, sizeof(Tmp->m_CurrentMap));
	str_copy(Tmp->m_ServerType, g_Config.m_SvServerType, sizeof(Tmp->m_ServerType));
	str_copy(Tmp->m_RequestingPlayer, GameServer()->Server()->ClientName(ClientID), sizeof(Tmp->m_RequestingPlayer));

	m_pPool->Execute(RandomMapThread, std::move(Tmp), "random map");
}

bool CScore::RandomMapThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlRandomMapRequest *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);

	char aBuf[512];
	if(0 <= pData->m_Stars && pData->m_Stars <= 5)
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Map FROM %s_maps "
			"WHERE Server = ? AND Map != ? AND Stars = ? "
			"ORDER BY RAND() LIMIT 1;",
			pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindInt(3, pData->m_Stars);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Map FROM %s_maps "
			"WHERE Server = ? AND Map != ? "
			"ORDER BY RAND() LIMIT 1;",
			pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
	}
	pSqlServer->BindString(1, pData->m_ServerType);
	pSqlServer->BindString(2, pData->m_CurrentMap);

	if(pSqlServer->Step())
	{
		pSqlServer->GetString(1, pData->m_pResult->m_Map, sizeof(pData->m_pResult->m_Map));
	}
	else
	{
		str_copy(pData->m_pResult->m_aMessage, "No maps found on this server!", sizeof(pData->m_pResult->m_aMessage));
	}

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::RandomUnfinishedMap(int ClientID, int Stars)
{
	auto pResult = std::make_shared<CScoreRandomMapResult>(ClientID);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto Tmp = std::unique_ptr<CSqlRandomMapRequest>(new CSqlRandomMapRequest(pResult));
	Tmp->m_Stars = Stars;
	str_copy(Tmp->m_CurrentMap, g_Config.m_SvMap, sizeof(Tmp->m_CurrentMap));
	str_copy(Tmp->m_ServerType, g_Config.m_SvServerType, sizeof(Tmp->m_ServerType));
	str_copy(Tmp->m_RequestingPlayer, GameServer()->Server()->ClientName(ClientID), sizeof(Tmp->m_RequestingPlayer));

	m_pPool->Execute(RandomUnfinishedMapThread, std::move(Tmp), "random unfinished map");
}

bool CScore::RandomUnfinishedMapThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlRandomMapRequest *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);

	char aBuf[512];
	if(pData->m_Stars >= 0)
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Map "
			"FROM %s_maps "
			"WHERE Server = ? AND Map != ? AND Stars = ? AND Map NOT IN ("
			"  SELECT Map "
			"  FROM %s_race "
			"  WHERE Name = ?"
			") ORDER BY RAND() "
			"LIMIT 1;",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_ServerType);
		pSqlServer->BindString(2, pData->m_CurrentMap);
		pSqlServer->BindInt(3, pData->m_Stars);
		pSqlServer->BindString(4, pData->m_RequestingPlayer);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Map "
			"FROM %s_maps AS maps "
			"WHERE Server = ? AND Map != ? AND Map NOT IN ("
			"  SELECT Map "
			"  FROM %s_race as race "
			"  WHERE Name = ?"
			") ORDER BY RAND() "
			"LIMIT 1;",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_ServerType);
		pSqlServer->BindString(2, pData->m_CurrentMap);
		pSqlServer->BindString(3, pData->m_RequestingPlayer);
	}

	if(pSqlServer->Step())
	{
		pSqlServer->GetString(1, pData->m_pResult->m_Map, sizeof(pData->m_pResult->m_Map));
	}
	else
	{
		str_copy(pData->m_pResult->m_aMessage, "You have no more unfinished maps on this server!", sizeof(pData->m_pResult->m_aMessage));
	}

	pData->m_pResult->m_Done = true;
	return true;
}

void CScore::SaveTeam(int ClientID, const char *Code, const char *Server)
{
	if(RateLimitPlayer(ClientID))
		return;
	auto pController = ((CGameControllerDDRace *)(GameServer()->m_pController));
	int Team = pController->m_Teams.m_Core.Team(ClientID);
	if(pController->m_Teams.GetSaving(Team))
		return;

	auto SaveResult = std::make_shared<CScoreSaveResult>(ClientID, pController);
	int Result = SaveResult->m_SavedTeam.save(Team);
	if(CSaveTeam::HandleSaveError(Result, ClientID, GameServer()))
		return;
	pController->m_Teams.SetSaving(Team, SaveResult);

	auto Tmp = std::unique_ptr<CSqlTeamSave>(new CSqlTeamSave(SaveResult));
	str_copy(Tmp->m_Code, Code, sizeof(Tmp->m_Code));
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));
	Tmp->m_pResult->m_SaveID = RandomUuid();
	str_copy(Tmp->m_Server, Server, sizeof(Tmp->m_Server));
	str_copy(Tmp->m_ClientName, this->Server()->ClientName(ClientID), sizeof(Tmp->m_ClientName));
	Tmp->m_aGeneratedCode[0] = '\0';
	GeneratePassphrase(Tmp->m_aGeneratedCode, sizeof(Tmp->m_aGeneratedCode));

	pController->m_Teams.KillSavedTeam(ClientID, Team);
	m_pPool->ExecuteWrite(SaveTeamThread, std::move(Tmp), "save team");
}

bool CScore::SaveTeamThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure)
{
	const CSqlTeamSave *pData = dynamic_cast<const CSqlTeamSave *>(pGameData);

	char aSaveID[UUID_MAXSTRSIZE];
	FormatUuid(pData->m_pResult->m_SaveID, aSaveID, UUID_MAXSTRSIZE);

	char *pSaveState = pData->m_pResult->m_SavedTeam.GetString();
	char aBuf[65536];

	char aTable[512];
	str_format(aTable, sizeof(aTable), "%s_saves WRITE", pSqlServer->GetPrefix());
	pSqlServer->Lock(aTable);

	char Code[128] = {0};
	str_format(aBuf, sizeof(aBuf), "SELECT Savegame FROM %s_saves WHERE Code = ? AND Map = ?", pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	bool UseCode = false;
	if(pData->m_Code[0] != '\0' && !Failure)
	{
		pSqlServer->BindString(1, pData->m_Code);
		pSqlServer->BindString(2, pData->m_Map);
		// only allow saving when save code does not already exist
		if(!pSqlServer->Step())
		{
			UseCode = true;
			str_copy(Code, pData->m_Code, sizeof(Code));
		}
	}
	if(!UseCode)
	{
		// use random generated passphrase if save code exists or no save code given
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		pSqlServer->BindString(2, pData->m_Map);
		if(!pSqlServer->Step())
		{
			UseCode = true;
			str_copy(Code, pData->m_aGeneratedCode, sizeof(Code));
		}
	}

	if(UseCode)
	{
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_saves(Savegame, Map, Code, Timestamp, Server, SaveID, DDNet7) "
			"VALUES (?, ?, ?, CURRENT_TIMESTAMP, ?, ?, false)",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pSaveState);
		pSqlServer->BindString(2, pData->m_Map);
		pSqlServer->BindString(3, Code);
		pSqlServer->BindString(4, pData->m_Server);
		pSqlServer->BindString(5, aSaveID);
		pSqlServer->Print();
		pSqlServer->Step();

		if(!Failure)
		{
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
		}
		else
		{
			strcpy(pData->m_pResult->m_aBroadcast,
				"Database connection failed, teamsave written to a file instead. Admins will add it manually in a few days.");
			if(str_comp(pData->m_Server, g_Config.m_SvSqlServerName) == 0)
			{
				str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
					"Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' to continue",
					pData->m_ClientName, Code);
			}
			else
			{
				str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
					"Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' on %s to continue",
					pData->m_ClientName, Code, pData->m_Server);
			}
		}

		pData->m_pResult->m_Status = CScoreSaveResult::SAVE_SUCCESS;
	}
	else
	{
		dbg_msg("sql", "ERROR: This save-code already exists");
		pData->m_pResult->m_Status = CScoreSaveResult::SAVE_FAILED;
		strcpy(pData->m_pResult->m_aMessage, "This save-code already exists");
	}

	pSqlServer->Unlock();
	return true;
}

void CScore::LoadTeam(const char *Code, int ClientID)
{
	if(RateLimitPlayer(ClientID))
		return;
	auto pController = ((CGameControllerDDRace *)(GameServer()->m_pController));
	int Team = pController->m_Teams.m_Core.Team(ClientID);
	if(pController->m_Teams.GetSaving(Team))
		return;
	if(Team < TEAM_FLOCK || Team >= MAX_CLIENTS || (g_Config.m_SvTeam != 3 && Team == TEAM_FLOCK))
	{
		GameServer()->SendChatTarget(ClientID, "You have to be in a team (from 1-63)");
		return;
	}
	if(pController->m_Teams.GetTeamState(Team) != CGameTeams::TEAMSTATE_OPEN)
	{
		GameServer()->SendChatTarget(ClientID, "Team can't be loaded while racing");
		return;
	}
	auto SaveResult = std::make_shared<CScoreSaveResult>(ClientID, pController);
	SaveResult->m_Status = CScoreSaveResult::LOAD_FAILED;
	pController->m_Teams.SetSaving(Team, SaveResult);
	auto Tmp = std::unique_ptr<CSqlTeamLoad>(new CSqlTeamLoad(SaveResult));
	str_copy(Tmp->m_Code, Code, sizeof(Tmp->m_Code));
	str_copy(Tmp->m_Map, g_Config.m_SvMap, sizeof(Tmp->m_Map));
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_RequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_RequestingPlayer));
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
	m_pPool->ExecuteWrite(LoadTeamThread, std::move(Tmp), "load team");
}

bool CScore::LoadTeamThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure)
{
	const CSqlTeamLoad *pData = dynamic_cast<const CSqlTeamLoad *>(pGameData);
	pData->m_pResult->m_Status = CScoreSaveResult::LOAD_FAILED;

	char aTable[512];
	str_format(aTable, sizeof(aTable), "%s_saves WRITE", pSqlServer->GetPrefix());
	pSqlServer->Lock(aTable);

	{
		char aSaveLike[128] = "";
		str_append(aSaveLike, "%\n", sizeof(aSaveLike));
		sqlstr::EscapeLike(aSaveLike + str_length(aSaveLike),
			pData->m_RequestingPlayer,
			sizeof(aSaveLike) - str_length(aSaveLike));
		str_append(aSaveLike, "\t%", sizeof(aSaveLike));

		char aCurrentTimestamp[512];
		pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
		char aTimestamp[512];
		pSqlServer->ToUnixTimestamp("Timestamp", aTimestamp, sizeof(aTimestamp));

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"SELECT Savegame, %s-%s AS Ago, SaveID "
			"FROM %s_saves "
			"where Code = ? AND Map = ? AND DDNet7 = false AND Savegame LIKE ?;",
			aCurrentTimestamp, aTimestamp,
			pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_Code);
		pSqlServer->BindString(2, pData->m_Map);
		pSqlServer->BindString(3, aSaveLike);

		if(!pSqlServer->Step())
		{
			strcpy(pData->m_pResult->m_aMessage, "No such savegame for this map");
			goto end;
		}

		int Since = pSqlServer->GetInt(2);
		if(Since < g_Config.m_SvSaveGamesDelay)
		{
			str_format(pData->m_pResult->m_aMessage, sizeof(pData->m_pResult->m_aMessage),
				"You have to wait %d seconds until you can load this savegame",
				g_Config.m_SvSaveGamesDelay - Since);
			goto end;
		}

		char aSaveID[UUID_MAXSTRSIZE];
		memset(pData->m_pResult->m_SaveID.m_aData, 0, sizeof(pData->m_pResult->m_SaveID.m_aData));
		if(!pSqlServer->IsNull(3))
		{
			pSqlServer->GetString(3, aSaveID, sizeof(aSaveID));
			if(str_length(aSaveID) + 1 != UUID_MAXSTRSIZE)
			{
				strcpy(pData->m_pResult->m_aMessage, "Unable to load savegame: SaveID corrupted");
				goto end;
			}
			ParseUuid(&pData->m_pResult->m_SaveID, aSaveID);
		}

		char aSaveString[65536];
		pSqlServer->GetString(1, aSaveString, sizeof(aSaveString));
		int Num = pData->m_pResult->m_SavedTeam.FromString(aSaveString);

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
			"WHERE Code = ? AND Map = ?;",
			pSqlServer->GetPrefix());
		pSqlServer->PrepareStatement(aBuf);
		pSqlServer->BindString(1, pData->m_Code);
		pSqlServer->BindString(2, pData->m_Map);
		pSqlServer->Print();
		pSqlServer->Step();

		pData->m_pResult->m_Status = CScoreSaveResult::LOAD_SUCCESS;
		strcpy(pData->m_pResult->m_aMessage, "Loading successfully done");
	}
end:
	pSqlServer->Unlock();
	return true;
}

void CScore::GetSaves(int ClientID)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(GetSavesThread, "get saves", ClientID, "", 0);
}

bool CScore::GetSavesThread(IDbConnection *pSqlServer, const ISqlData *pGameData)
{
	const CSqlPlayerRequest *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto paMessages = pData->m_pResult->m_Data.m_aaMessages;

	char aSaveLike[128] = "";
	str_append(aSaveLike, "%\n", sizeof(aSaveLike));
	sqlstr::EscapeLike(aSaveLike + str_length(aSaveLike),
		pData->m_RequestingPlayer,
		sizeof(aSaveLike) - str_length(aSaveLike));
	str_append(aSaveLike, "\t%", sizeof(aSaveLike));

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aMaxTimestamp[512];
	pSqlServer->ToUnixTimestamp("MAX(Timestamp)", aMaxTimestamp, sizeof(aMaxTimestamp));

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT COUNT(*) AS NumSaves, %s-%s AS Ago "
		"FROM %s_saves "
		"WHERE Map = ? AND Savegame LIKE ?;",
		aCurrentTimestamp, aMaxTimestamp,
		pSqlServer->GetPrefix());
	pSqlServer->PrepareStatement(aBuf);
	pSqlServer->BindString(1, pData->m_Map);
	pSqlServer->BindString(2, aSaveLike);

	if(pSqlServer->Step())
	{
		int NumSaves = pSqlServer->GetInt(1);
		int Ago = pSqlServer->GetInt(2);
		char aAgoString[40] = "\0";
		char aLastSavedString[60] = "\0";
		if(Ago)
		{
			sqlstr::AgoTimeToString(Ago, aAgoString);
			str_format(aLastSavedString, sizeof(aLastSavedString), ", last saved %s ago", aAgoString);
		}

		str_format(paMessages[0], sizeof(paMessages[0]),
			"%s has %d save%s on %s%s",
			pData->m_RequestingPlayer,
			NumSaves, NumSaves == 1 ? "" : "s",
			pData->m_Map, aLastSavedString);
	}

	pData->m_pResult->m_Done = true;
	return true;
}
