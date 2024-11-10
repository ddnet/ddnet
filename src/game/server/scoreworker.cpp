#include "scoreworker.h"

#include <base/log.h>
#include <base/system.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/sql_string_helpers.h>
#include <engine/shared/config.h>

#include <cmath>

// "6b407e81-8b77-3e04-a207-8da17f37d000"
// "save-no-save-id@ddnet.tw"
static const CUuid UUID_NO_SAVE_ID =
	{{0x6b, 0x40, 0x7e, 0x81, 0x8b, 0x77, 0x3e, 0x04,
		0xa2, 0x07, 0x8d, 0xa1, 0x7f, 0x37, 0xd0, 0x00}};

CScorePlayerResult::CScorePlayerResult()
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
		for(auto &aMessage : m_Data.m_aaMessages)
			aMessage[0] = 0;
		break;
	case BROADCAST:
		m_Data.m_aBroadcast[0] = 0;
		break;
	case MAP_VOTE:
		m_Data.m_MapVote.m_aMap[0] = '\0';
		m_Data.m_MapVote.m_aReason[0] = '\0';
		m_Data.m_MapVote.m_aServer[0] = '\0';
		break;
	case PLAYER_INFO:
		m_Data.m_Info.m_Birthday = 0;
		m_Data.m_Info.m_Time.reset();
		for(float &TimeCp : m_Data.m_Info.m_aTimeCp)
			TimeCp = 0;
		break;
	case PLAYER_TIMECP:
		m_Data.m_Info.m_aRequestedPlayer[0] = '\0';
		m_Data.m_Info.m_Time.reset();
		for(float &TimeCp : m_Data.m_Info.m_aTimeCp)
			TimeCp = 0;
		break;
	}
}

CTeamrank::CTeamrank() :
	m_NumNames(0)
{
	for(auto &aName : m_aaNames)
		aName[0] = '\0';
	mem_zero(&m_TeamId.m_aData, sizeof(m_TeamId));
}

bool CTeamrank::NextSqlResult(IDbConnection *pSqlServer, bool *pEnd, char *pError, int ErrorSize)
{
	pSqlServer->GetBlob(1, m_TeamId.m_aData, sizeof(m_TeamId.m_aData));
	pSqlServer->GetString(2, m_aaNames[0], sizeof(m_aaNames[0]));
	m_NumNames = 1;
	bool End = false;
	while(!pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		CUuid TeamId;
		pSqlServer->GetBlob(1, TeamId.m_aData, sizeof(TeamId.m_aData));
		if(m_TeamId != TeamId)
		{
			*pEnd = false;
			return false;
		}
		pSqlServer->GetString(2, m_aaNames[m_NumNames], sizeof(m_aaNames[m_NumNames]));
		m_NumNames++;
	}
	if(!End)
	{
		return true;
	}
	*pEnd = true;
	return false;
}

bool CTeamrank::SamePlayers(const std::vector<std::string> *pvSortedNames)
{
	if(pvSortedNames->size() != m_NumNames)
		return false;
	for(unsigned int i = 0; i < m_NumNames; i++)
	{
		if(str_comp(pvSortedNames->at(i).c_str(), m_aaNames[i]) != 0)
			return false;
	}
	return true;
}

bool CTeamrank::GetSqlTop5Team(IDbConnection *pSqlServer, bool *pEnd, char *pError, int ErrorSize, char (*paMessages)[512], int *Line, int Count)
{
	char aTime[32];
	int StartLine = *Line;
	for(*Line = StartLine; *Line < StartLine + Count; (*Line)++)
	{
		bool Last = false;
		float Time = pSqlServer->GetFloat(2);
		str_time_float(Time, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
		int Rank = pSqlServer->GetInt(3);
		int TeamSize = pSqlServer->GetInt(4);

		char aNames[2300] = {0};
		for(int i = 0; i < TeamSize; i++)
		{
			char aName[MAX_NAME_LENGTH];
			pSqlServer->GetString(1, aName, sizeof(aName));
			str_append(aNames, aName);
			if(i < TeamSize - 2)
				str_append(aNames, ", ");
			else if(i == TeamSize - 2)
				str_append(aNames, " & ");
			if(pSqlServer->Step(&Last, pError, ErrorSize))
			{
				return true;
			}
			if(Last)
			{
				break;
			}
		}
		str_format(paMessages[*Line], sizeof(paMessages[*Line]), "%d. %s Team Time: %s",
			Rank, aNames, aTime);
		if(Last)
		{
			(*Line)++;
			break;
		}
	}
	return false;
}

bool CScoreWorker::LoadBestTime(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlLoadBestTimeRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreLoadBestTimeResult *>(pGameData->m_pResult.get());

	char aBuf[512];
	// get the best time
	str_format(aBuf, sizeof(aBuf),
		"SELECT Time FROM %s_race WHERE Map=? ORDER BY `Time` ASC LIMIT 1",
		pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		pResult->m_CurrentRecord = pSqlServer->GetFloat(1);
	}

	return false;
}

// update stuff
bool CScoreWorker::LoadPlayerData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	pResult->SetVariant(CScorePlayerResult::PLAYER_INFO);

	char aBuf[1024];
	// get best race time
	str_format(aBuf, sizeof(aBuf),
		"SELECT"
		"  (SELECT Time FROM %s_race WHERE Map = ? AND Name = ? ORDER BY Time ASC LIMIT 1) AS minTime, "
		"  cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, "
		"  cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, "
		"  (cp1 + cp2 + cp3 + cp4 + cp5 + cp6 + cp7 + cp8 + cp9 + cp10 + cp11 + cp12 + cp13 + cp14 + "
		"  cp15 + cp16 + cp17 + cp18 + cp19 + cp20 + cp21 + cp22 + cp23 + cp24 + cp25 > 0) AS hasCP, Time "
		"FROM %s_race "
		"WHERE Map = ? AND Name = ? "
		"ORDER BY hasCP DESC, Time ASC "
		"LIMIT 1",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}

	const char *pPlayer = pData->m_aName[0] != '\0' ? pData->m_aName : pData->m_aRequestingPlayer;
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pData->m_aRequestingPlayer);
	pSqlServer->BindString(3, pData->m_aMap);
	pSqlServer->BindString(4, pPlayer);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		if(!pSqlServer->IsNull(1))
		{
			// get the best time
			float Time = pSqlServer->GetFloat(1);
			pResult->m_Data.m_Info.m_Time = Time;
		}

		for(int i = 0; i < NUM_CHECKPOINTS; i++)
		{
			pResult->m_Data.m_Info.m_aTimeCp[i] = pSqlServer->GetFloat(i + 2);
		}
	}

	// birthday check
	str_format(aBuf, sizeof(aBuf),
		"SELECT CURRENT_TIMESTAMP AS Current, MIN(Timestamp) AS Stamp "
		"FROM %s_race "
		"WHERE Name = ?",
		pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aRequestingPlayer);

	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End && !pSqlServer->IsNull(2))
	{
		char aCurrent[TIMESTAMP_STR_LENGTH];
		pSqlServer->GetString(1, aCurrent, sizeof(aCurrent));
		char aStamp[TIMESTAMP_STR_LENGTH];
		pSqlServer->GetString(2, aStamp, sizeof(aStamp));
		int CurrentYear, CurrentMonth, CurrentDay;
		int StampYear, StampMonth, StampDay;
		if(sscanf(aCurrent, "%d-%d-%d", &CurrentYear, &CurrentMonth, &CurrentDay) == 3 && sscanf(aStamp, "%d-%d-%d", &StampYear, &StampMonth, &StampDay) == 3 && CurrentMonth == StampMonth && CurrentDay == StampDay)
			pResult->m_Data.m_Info.m_Birthday = CurrentYear - StampYear;
	}
	return false;
}

bool CScoreWorker::LoadPlayerTimeCp(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf),
		"SELECT"
		"  Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, "
		"  cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25 "
		"FROM %s_race "
		"WHERE Map = ? AND Name = ? AND "
		"  (cp1 + cp2 + cp3 + cp4 + cp5 + cp6 + cp7 + cp8 + cp9 + cp10 + cp11 + cp12 + cp13 + cp14 + "
		"  cp15 + cp16 + cp17 + cp18 + cp19 + cp20 + cp21 + cp22 + cp23 + cp24 + cp25) > 0 "
		"ORDER BY Time ASC "
		"LIMIT 1",
		pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}

	const char *pPlayer = pData->m_aName[0] != '\0' ? pData->m_aName : pData->m_aRequestingPlayer;
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pPlayer);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		pResult->SetVariant(CScorePlayerResult::PLAYER_TIMECP);
		pResult->m_Data.m_Info.m_Time = pSqlServer->GetFloat(1);
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
		{
			pResult->m_Data.m_Info.m_aTimeCp[i] = pSqlServer->GetFloat(i + 2);
		}
		str_copy(pResult->m_Data.m_Info.m_aRequestedPlayer, pPlayer, sizeof(pResult->m_Data.m_Info.m_aRequestedPlayer));
	}
	else
	{
		pResult->SetVariant(CScorePlayerResult::DIRECT);
		str_format(paMessages[0], sizeof(paMessages[0]), "'%s' has no checkpoint times available", pPlayer);
	}
	return false;
}

bool CScoreWorker::MapVote(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aFuzzyMap[128];
	str_copy(aFuzzyMap, pData->m_aName, sizeof(aFuzzyMap));
	sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

	char aMapPrefix[128];
	str_copy(aMapPrefix, pData->m_aName, sizeof(aMapPrefix));
	str_append(aMapPrefix, "%");

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Map, Server "
		"FROM %s_maps "
		"WHERE Map LIKE %s "
		"ORDER BY "
		"  CASE WHEN Map = ? THEN 0 ELSE 1 END, "
		"  CASE WHEN Map LIKE ? THEN 0 ELSE 1 END, "
		"  LENGTH(Map), Map "
		"LIMIT 1",
		pSqlServer->GetPrefix(), pSqlServer->CollateNocase());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, aFuzzyMap);
	pSqlServer->BindString(2, pData->m_aName);
	pSqlServer->BindString(3, aMapPrefix);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		pResult->SetVariant(CScorePlayerResult::MAP_VOTE);
		auto *pMapVote = &pResult->m_Data.m_MapVote;
		pSqlServer->GetString(1, pMapVote->m_aMap, sizeof(pMapVote->m_aMap));
		pSqlServer->GetString(2, pMapVote->m_aServer, sizeof(pMapVote->m_aServer));
		str_copy(pMapVote->m_aReason, "/map", sizeof(pMapVote->m_aReason));

		for(char *p = pMapVote->m_aServer; *p; p++) // lower case server
			*p = tolower(*p);
	}
	else
	{
		pResult->SetVariant(CScorePlayerResult::DIRECT);
		str_format(paMessages[0], sizeof(paMessages[0]),
			"No map like \"%s\" found. "
			"Try adding a '%%' at the start if you don't know the first character. "
			"Example: /map %%castle for \"Out of Castle\"",
			pData->m_aName);
	}
	return false;
}

bool CScoreWorker::MapInfo(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	char aFuzzyMap[128];
	str_copy(aFuzzyMap, pData->m_aName, sizeof(aFuzzyMap));
	sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

	char aMapPrefix[128];
	str_copy(aMapPrefix, pData->m_aName, sizeof(aMapPrefix));
	str_append(aMapPrefix, "%");

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("l.Timestamp", aTimestamp, sizeof(aTimestamp));

	char aMedianMapTime[2048];
	char aBuf[4096];
	str_format(aBuf, sizeof(aBuf),
		"SELECT l.Map, l.Server, Mapper, Points, Stars, "
		"  (SELECT COUNT(Name) FROM %s_race WHERE Map = l.Map) AS Finishes, "
		"  (SELECT COUNT(DISTINCT Name) FROM %s_race WHERE Map = l.Map) AS Finishers, "
		"  (%s) AS Median, "
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
		") as l",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
		pSqlServer->MedianMapTime(aMedianMapTime, sizeof(aMedianMapTime)),
		aTimestamp, aCurrentTimestamp, aTimestamp,
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
		pSqlServer->CollateNocase());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aRequestingPlayer);
	pSqlServer->BindString(2, aFuzzyMap);
	pSqlServer->BindString(3, pData->m_aName);
	pSqlServer->BindString(4, aMapPrefix);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
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
		float Median = !pSqlServer->IsNull(8) ? pSqlServer->GetInt(8) : -1.0f;
		int Stamp = pSqlServer->GetInt(9);
		int Ago = pSqlServer->GetInt(10);
		float OwnTime = !pSqlServer->IsNull(11) ? pSqlServer->GetFloat(11) : -1.0f;

		char aAgoString[40] = "\0";
		char aReleasedString[60] = "\0";
		if(Stamp != 0)
		{
			sqlstr::AgoTimeToString(Ago, aAgoString, sizeof(aAgoString));
			str_format(aReleasedString, sizeof(aReleasedString), ", released %s ago", aAgoString);
		}

		char aMedianString[60] = "\0";
		if(Median > 0)
		{
			str_time((int64_t)Median * 100, TIME_HOURS, aBuf, sizeof(aBuf));
			str_format(aMedianString, sizeof(aMedianString), " in %s median", aBuf);
		}

		char aStars[20];
		switch(Stars)
		{
		case 0: str_copy(aStars, "✰✰✰✰✰", sizeof(aStars)); break;
		case 1: str_copy(aStars, "★✰✰✰✰", sizeof(aStars)); break;
		case 2: str_copy(aStars, "★★✰✰✰", sizeof(aStars)); break;
		case 3: str_copy(aStars, "★★★✰✰", sizeof(aStars)); break;
		case 4: str_copy(aStars, "★★★★✰", sizeof(aStars)); break;
		case 5: str_copy(aStars, "★★★★★", sizeof(aStars)); break;
		default: aStars[0] = '\0';
		}

		char aOwnFinishesString[40] = "\0";
		if(OwnTime > 0)
		{
			str_time_float(OwnTime, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
			str_format(aOwnFinishesString, sizeof(aOwnFinishesString),
				", your time: %s", aBuf);
		}

		str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
			"\"%s\" by %s on %s, %s, %d %s%s, %d %s by %d %s%s%s",
			aMap, aMapper, aServer, aStars,
			Points, Points == 1 ? "point" : "points",
			aReleasedString,
			Finishes, Finishes == 1 ? "finish" : "finishes",
			Finishers, Finishers == 1 ? "tee" : "tees",
			aMedianString, aOwnFinishesString);
	}
	else
	{
		str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
			"No map like \"%s\" found.", pData->m_aName);
	}
	return false;
}

bool CScoreWorker::SaveScore(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlScoreData *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aBuf[1024];

	if(w == Write::NORMAL_SUCCEEDED)
	{
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_race_backup WHERE GameId=? AND Name=? AND Timestamp=%s",
			pSqlServer->GetPrefix(), pSqlServer->InsertTimestampAsUtc());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aGameUuid);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		pSqlServer->Print();
		int NumDeleted;
		if(pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize))
		{
			return true;
		}
		if(NumDeleted == 0)
		{
			log_warn("sql", "Rank got moved out of backup database, will show up as duplicate rank in MySQL");
		}
		return false;
	}
	if(w == Write::NORMAL_FAILED)
	{
		int NumUpdated;
		// move to non-tmp table succeeded. delete from backup again
		str_format(aBuf, sizeof(aBuf),
			"INSERT INTO %s_race SELECT * FROM %s_race_backup WHERE GameId=? AND Name=? AND Timestamp=%s",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->InsertTimestampAsUtc());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aGameUuid);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		pSqlServer->Print();
		if(pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		{
			return true;
		}

		// move to non-tmp table succeeded. delete from backup again
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_race_backup WHERE GameId=? AND Name=? AND Timestamp=%s",
			pSqlServer->GetPrefix(), pSqlServer->InsertTimestampAsUtc());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aGameUuid);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		pSqlServer->Print();
		if(pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		{
			return true;
		}
		if(NumUpdated == 0)
		{
			log_warn("sql", "Rank got moved out of backup database, will show up as duplicate rank in MySQL");
		}
		return false;
	}

	if(w == Write::NORMAL)
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT COUNT(*) AS NumFinished FROM %s_race WHERE Map=? AND Name=? ORDER BY time ASC LIMIT 1",
			pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aMap);
		pSqlServer->BindString(2, pData->m_aName);

		bool End;
		if(pSqlServer->Step(&End, pError, ErrorSize))
		{
			return true;
		}
		int NumFinished = pSqlServer->GetInt(1);
		if(NumFinished == 0)
		{
			str_format(aBuf, sizeof(aBuf), "SELECT Points FROM %s_maps WHERE Map=?", pSqlServer->GetPrefix());
			if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
			{
				return true;
			}
			pSqlServer->BindString(1, pData->m_aMap);

			bool End2;
			if(pSqlServer->Step(&End2, pError, ErrorSize))
			{
				return true;
			}
			if(!End2)
			{
				int Points = pSqlServer->GetInt(1);
				if(pSqlServer->AddPoints(pData->m_aName, Points, pError, ErrorSize))
				{
					return true;
				}
				str_format(paMessages[0], sizeof(paMessages[0]),
					"You earned %d point%s for finishing this map!",
					Points, Points == 1 ? "" : "s");
			}
		}
	}

	// save score. Can't fail, because no UNIQUE/PRIMARY KEY constrain is defined.
	str_format(aBuf, sizeof(aBuf),
		"%s INTO %s_race%s("
		"	Map, Name, Timestamp, Time, Server, "
		"	cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, "
		"	cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25, "
		"	GameId, DDNet7) "
		"VALUES (?, ?, %s, %.2f, ?, "
		"	%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, "
		"	%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, "
		"	%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, "
		"	?, %s)",
		pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
		w == Write::NORMAL ? "" : "_backup",
		pSqlServer->InsertTimestampAsUtc(), pData->m_Time,
		pData->m_aCurrentTimeCp[0], pData->m_aCurrentTimeCp[1], pData->m_aCurrentTimeCp[2],
		pData->m_aCurrentTimeCp[3], pData->m_aCurrentTimeCp[4], pData->m_aCurrentTimeCp[5],
		pData->m_aCurrentTimeCp[6], pData->m_aCurrentTimeCp[7], pData->m_aCurrentTimeCp[8],
		pData->m_aCurrentTimeCp[9], pData->m_aCurrentTimeCp[10], pData->m_aCurrentTimeCp[11],
		pData->m_aCurrentTimeCp[12], pData->m_aCurrentTimeCp[13], pData->m_aCurrentTimeCp[14],
		pData->m_aCurrentTimeCp[15], pData->m_aCurrentTimeCp[16], pData->m_aCurrentTimeCp[17],
		pData->m_aCurrentTimeCp[18], pData->m_aCurrentTimeCp[19], pData->m_aCurrentTimeCp[20],
		pData->m_aCurrentTimeCp[21], pData->m_aCurrentTimeCp[22], pData->m_aCurrentTimeCp[23],
		pData->m_aCurrentTimeCp[24], pSqlServer->False());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pData->m_aName);
	pSqlServer->BindString(3, pData->m_aTimestamp);
	pSqlServer->BindString(4, g_Config.m_SvSqlServerName);
	pSqlServer->BindString(5, pData->m_aGameUuid);
	pSqlServer->Print();
	int NumInserted;
	return pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize);
}

bool CScoreWorker::SaveTeamScore(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlTeamScoreData *>(pGameData);

	char aBuf[512];

	if(w == Write::NORMAL_SUCCEEDED)
	{
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_teamrace_backup WHERE Id=?",
			pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}

		// copy uuid, because mysql BindBlob doesn't support const buffers
		CUuid TeamrankId = pData->m_TeamrankUuid;
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->Print();
		int NumDeleted;
		if(pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize))
		{
			return true;
		}
		if(NumDeleted == 0)
		{
			log_warn("sql", "Teamrank got moved out of backup database, will show up as duplicate teamrank in MySQL");
		}
		return false;
	}
	if(w == Write::NORMAL_FAILED)
	{
		int NumInserted;
		CUuid TeamrankId = pData->m_TeamrankUuid;

		str_format(aBuf, sizeof(aBuf),
			"INSERT INTO %s_teamrace SELECT * FROM %s_teamrace_backup WHERE Id=?",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->Print();
		if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return true;
		}

		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_teamrace_backup WHERE Id=?",
			pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->Print();
		return pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize);
	}

	if(w == Write::NORMAL)
	{
		// get the names sorted in a tab separated string
		std::vector<std::string> vNames;
		for(unsigned int i = 0; i < pData->m_Size; i++)
			vNames.emplace_back(pData->m_aaNames[i]);

		std::sort(vNames.begin(), vNames.end());
		str_format(aBuf, sizeof(aBuf),
			"SELECT l.Id, Name, Time "
			"FROM (" // preselect teams with first name in team
			"  SELECT ID "
			"  FROM %s_teamrace "
			"  WHERE Map = ? AND Name = ? AND DDNet7 = %s"
			") as l INNER JOIN %s_teamrace AS r ON l.Id = r.Id "
			"ORDER BY l.Id, Name COLLATE %s",
			pSqlServer->GetPrefix(), pSqlServer->False(), pSqlServer->GetPrefix(), pSqlServer->BinaryCollate());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aMap);
		pSqlServer->BindString(2, pData->m_aaNames[0]);

		bool FoundTeam = false;
		float Time;
		CTeamrank Teamrank;
		bool End;
		if(pSqlServer->Step(&End, pError, ErrorSize))
		{
			return true;
		}
		if(!End)
		{
			bool SearchTeamEnd = false;
			while(!SearchTeamEnd)
			{
				Time = pSqlServer->GetFloat(3);
				if(Teamrank.NextSqlResult(pSqlServer, &SearchTeamEnd, pError, ErrorSize))
				{
					return true;
				}
				if(Teamrank.SamePlayers(&vNames))
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
					"UPDATE %s_teamrace SET Time=%.2f, Timestamp=%s, DDNet7=%s, GameId=? WHERE Id = ?",
					pSqlServer->GetPrefix(), pData->m_Time, pSqlServer->InsertTimestampAsUtc(), pSqlServer->False());
				if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
				{
					return true;
				}
				pSqlServer->BindString(1, pData->m_aTimestamp);
				pSqlServer->BindString(2, pData->m_aGameUuid);
				pSqlServer->BindBlob(3, Teamrank.m_TeamId.m_aData, sizeof(Teamrank.m_TeamId.m_aData));
				pSqlServer->Print();
				int NumUpdated;
				if(pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
				{
					return true;
				}
				// return error if we didn't update any rows
				return NumUpdated == 0;
			}
			return false;
		}
	}

	for(unsigned int i = 0; i < pData->m_Size; i++)
	{
		// if no entry found... create a new one
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_teamrace%s(Map, Name, Timestamp, Time, Id, GameId, DDNet7) "
			"VALUES (?, ?, %s, %.2f, ?, ?, %s)",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
			w == Write::NORMAL ? "" : "_backup",
			pSqlServer->InsertTimestampAsUtc(), pData->m_Time, pSqlServer->False());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aMap);
		pSqlServer->BindString(2, pData->m_aaNames[i]);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		// copy uuid, because mysql BindBlob doesn't support const buffers
		CUuid TeamrankId = pData->m_TeamrankUuid;
		pSqlServer->BindBlob(4, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->BindString(5, pData->m_aGameUuid);
		pSqlServer->Print();
		int NumInserted;
		if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return true;
		}
	}
	return false;
}

bool CScoreWorker::ShowRank(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	char aServerLike[16];
	str_format(aServerLike, sizeof(aServerLike), "%%%s%%", pData->m_aServer);

	// check sort method
	char aBuf[600];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Ranking, Time, PercentRank "
		"FROM ("
		"  SELECT RANK() OVER w AS Ranking, PERCENT_RANK() OVER w as PercentRank, MIN(Time) AS Time, Name "
		"  FROM %s_race "
		"  WHERE Map = ? "
		"  AND Server LIKE ? "
		"  GROUP BY Name "
		"  WINDOW w AS (ORDER BY MIN(Time))"
		") as a "
		"WHERE Name = ?",
		pSqlServer->GetPrefix());

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, aServerLike);
	pSqlServer->BindString(3, pData->m_aName);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}

	char aRegionalRank[16];
	if(End)
	{
		str_copy(aRegionalRank, "unranked", sizeof(aRegionalRank));
	}
	else
	{
		str_format(aRegionalRank, sizeof(aRegionalRank), "rank %d", pSqlServer->GetInt(1));
	}

	const char *pAny = "%";

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pAny);
	pSqlServer->BindString(3, pData->m_aName);

	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}

	if(!End)
	{
		int Rank = pSqlServer->GetInt(1);
		float Time = pSqlServer->GetFloat(2);
		// CEIL and FLOOR are not supported in SQLite
		int BetterThanPercent = std::floor(100.0f - 100.0f * pSqlServer->GetFloat(3));
		str_time_float(Time, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
		if(g_Config.m_SvHideScore)
		{
			str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
				"Your time: %s, better than %d%%", aBuf, BetterThanPercent);
		}
		else
		{
			pResult->m_MessageKind = CScorePlayerResult::ALL;

			if(str_comp_nocase(pData->m_aRequestingPlayer, pData->m_aName) == 0)
			{
				str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
					"%s - %s - better than %d%%",
					pData->m_aName, aBuf, BetterThanPercent);
			}
			else
			{
				str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
					"%s - %s - better than %d%% - requested by %s",
					pData->m_aName, aBuf, BetterThanPercent, pData->m_aRequestingPlayer);
			}

			if(g_Config.m_SvRegionalRankings)
			{
				str_format(pResult->m_Data.m_aaMessages[1], sizeof(pResult->m_Data.m_aaMessages[1]),
					"Global rank %d - %s %s",
					Rank, pData->m_aServer, aRegionalRank);
			}
			else
			{
				str_format(pResult->m_Data.m_aaMessages[1], sizeof(pResult->m_Data.m_aaMessages[1]),
					"Global rank %d", Rank);
			}
		}
	}
	else
	{
		str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
			"%s is not ranked", pData->m_aName);
	}
	return false;
}

bool CScoreWorker::ShowTeamRank(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	// check sort method
	char aBuf[2400];

	str_format(aBuf, sizeof(aBuf),
		"SELECT l.Id, Name, Time, Ranking, PercentRank "
		"FROM (" // teamrank score board
		"  SELECT RANK() OVER w AS Ranking, PERCENT_RANK() OVER w AS PercentRank, Id "
		"  FROM %s_teamrace "
		"  WHERE Map = ? "
		"  GROUP BY ID "
		"  WINDOW w AS (ORDER BY Min(Time))"
		") AS TeamRank INNER JOIN (" // select rank with Name in team
		"  SELECT ID "
		"  FROM %s_teamrace "
		"  WHERE Map = ? AND Name = ? "
		"  ORDER BY Time "
		"  LIMIT 1"
		") AS l ON TeamRank.Id = l.Id "
		"INNER JOIN %s_teamrace AS r ON l.Id = r.Id ",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pData->m_aMap);
	pSqlServer->BindString(3, pData->m_aName);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		float Time = pSqlServer->GetFloat(3);
		str_time_float(Time, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
		int Rank = pSqlServer->GetInt(4);
		// CEIL and FLOOR are not supported in SQLite
		int BetterThanPercent = std::floor(100.0f - 100.0f * pSqlServer->GetFloat(5));
		CTeamrank Teamrank;
		if(Teamrank.NextSqlResult(pSqlServer, &End, pError, ErrorSize))
		{
			return true;
		}

		char aFormattedNames[512] = "";
		for(unsigned int Name = 0; Name < Teamrank.m_NumNames; Name++)
		{
			str_append(aFormattedNames, Teamrank.m_aaNames[Name]);

			if(Name < Teamrank.m_NumNames - 2)
				str_append(aFormattedNames, ", ");
			else if(Name < Teamrank.m_NumNames - 1)
				str_append(aFormattedNames, " & ");
		}

		if(g_Config.m_SvHideScore)
		{
			str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
				"Your team time: %s, better than %d%%", aBuf, BetterThanPercent);
		}
		else
		{
			pResult->m_MessageKind = CScorePlayerResult::ALL;
			str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
				"%d. %s Team time: %s, better than %d%%, requested by %s",
				Rank, aFormattedNames, aBuf, BetterThanPercent, pData->m_aRequestingPlayer);
		}
	}
	else
	{
		str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
			"%s has no team ranks", pData->m_aName);
	}
	return false;
}

bool CScoreWorker::ShowTop(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	int LimitStart = maximum(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";
	const char *pAny = "%";

	// check sort method
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Name, Time, Ranking "
		"FROM ("
		"  SELECT RANK() OVER w AS Ranking, MIN(Time) AS Time, Name "
		"  FROM %s_race "
		"  WHERE Map = ? "
		"  AND Server LIKE ? "
		"  GROUP BY Name "
		"  WINDOW w AS (ORDER BY MIN(Time))"
		") as a "
		"ORDER BY Ranking %s "
		"LIMIT %d, ?",
		pSqlServer->GetPrefix(),
		pOrder,
		LimitStart);

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pAny);
	pSqlServer->BindInt(3, 5);

	// show top
	int Line = 0;
	str_copy(pResult->m_Data.m_aaMessages[Line], "------------ Global Top ------------", sizeof(pResult->m_Data.m_aaMessages[Line]));
	Line++;

	char aTime[32];
	bool End = false;

	while(!pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(1, aName, sizeof(aName));
		float Time = pSqlServer->GetFloat(2);
		str_time_float(Time, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
		int Rank = pSqlServer->GetInt(3);
		str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
			"%d. %s Time: %s", Rank, aName, aTime);

		Line++;
	}

	if(!g_Config.m_SvRegionalRankings)
	{
		str_copy(pResult->m_Data.m_aaMessages[Line], "-----------------------------------------", sizeof(pResult->m_Data.m_aaMessages[Line]));
		return !End;
	}

	char aServerLike[16];
	str_format(aServerLike, sizeof(aServerLike), "%%%s%%", pData->m_aServer);

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, aServerLike);
	pSqlServer->BindInt(3, 3);

	str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
		"------------ %s Top ------------", pData->m_aServer);
	Line++;

	// show top
	while(!pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(1, aName, sizeof(aName));
		float Time = pSqlServer->GetFloat(2);
		str_time_float(Time, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
		int Rank = pSqlServer->GetInt(3);
		str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
			"%d. %s Time: %s", Rank, aName, aTime);
		Line++;
	}

	return !End;
}

bool CScoreWorker::ShowTeamTop5(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";
	const char *pAny = "%";

	// check sort method
	char aBuf[1024];

	str_format(aBuf, sizeof(aBuf),
		"SELECT Name, Time, Ranking, TeamSize "
		"FROM ("
		"  SELECT TeamSize, Ranking, Id, Server "
		"  FROM (" // teamrank score board
		"    SELECT RANK() OVER w AS Ranking, COUNT(*) AS Teamsize, Id, Server "
		"    FROM ("
		"      SELECT tr.Map, tr.Time, tr.Id, rr.Server FROM %s_teamrace as tr "
		"      INNER JOIN %s_race as rr ON tr.Map = rr.Map AND tr.Name = rr.Name AND tr.Time = rr.Time"
		"    ) AS ll "
		"    WHERE Map = ? "
		"    GROUP BY ID "
		"    WINDOW w AS (ORDER BY Min(Time))"
		"  ) as l1 "
		"  WHERE Server LIKE ? "
		"  ORDER BY Ranking %s "
		"  LIMIT %d, ?"
		") as l2 "
		"INNER JOIN %s_teamrace as r ON l2.Id = r.Id "
		"ORDER BY Ranking %s, r.Id, Name ASC",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pOrder, LimitStart, pSqlServer->GetPrefix(), pOrder);
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pAny);
	pSqlServer->BindInt(3, 5);

	int Line = 0;
	str_copy(paMessages[Line++], "------- Team Top 5 -------", sizeof(paMessages[Line]));

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		if(CTeamrank::GetSqlTop5Team(pSqlServer, &End, pError, ErrorSize, paMessages, &Line, 5))
		{
			return true;
		}
	}

	if(!g_Config.m_SvRegionalRankings)
	{
		str_copy(paMessages[Line], "-------------------------------", sizeof(paMessages[Line]));
		return false;
	}

	char aServerLike[16];
	str_format(aServerLike, sizeof(aServerLike), "%%%s%%", pData->m_aServer);

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, aServerLike);
	pSqlServer->BindInt(3, 3);

	str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
		"----- %s Team Top -----", pData->m_aServer);
	Line++;

	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		if(CTeamrank::GetSqlTop5Team(pSqlServer, &End, pError, ErrorSize, paMessages, &Line, 3))
		{
			return true;
		}
	}
	return false;
}

bool CScoreWorker::ShowPlayerTeamTop5(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	// check sort method
	char aBuf[2400];

	str_format(aBuf, sizeof(aBuf),
		"SELECT l.Id, Name, Time, Ranking "
		"FROM (" // teamrank score board
		"  SELECT RANK() OVER w AS Ranking, Id "
		"  FROM %s_teamrace "
		"  WHERE Map = ? "
		"  GROUP BY ID "
		"  WINDOW w AS (ORDER BY Min(Time))"
		") AS TeamRank INNER JOIN (" // select rank with Name in team
		"  SELECT ID "
		"  FROM %s_teamrace "
		"  WHERE Map = ? AND Name = ? "
		"  ORDER BY Time %s "
		"  LIMIT %d, 5 "
		") AS l ON TeamRank.Id = l.Id "
		"INNER JOIN %s_teamrace AS r ON l.Id = r.Id "
		"ORDER BY Time %s, l.Id, Name ASC",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pOrder, LimitStart, pSqlServer->GetPrefix(), pOrder);
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, pData->m_aMap);
	pSqlServer->BindString(3, pData->m_aName);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		// show teamtop5
		int Line = 0;
		str_copy(paMessages[Line++], "------- Team Top 5 -------", sizeof(paMessages[Line]));

		for(Line = 1; Line < 6; Line++) // print
		{
			float Time = pSqlServer->GetFloat(3);
			str_time_float(Time, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
			int Rank = pSqlServer->GetInt(4);
			CTeamrank Teamrank;
			bool Last;
			if(Teamrank.NextSqlResult(pSqlServer, &Last, pError, ErrorSize))
			{
				return true;
			}

			char aFormattedNames[512] = "";
			for(unsigned int Name = 0; Name < Teamrank.m_NumNames; Name++)
			{
				str_append(aFormattedNames, Teamrank.m_aaNames[Name]);

				if(Name < Teamrank.m_NumNames - 2)
					str_append(aFormattedNames, ", ");
				else if(Name < Teamrank.m_NumNames - 1)
					str_append(aFormattedNames, " & ");
			}

			str_format(paMessages[Line], sizeof(paMessages[Line]), "%d. %s Team Time: %s",
				Rank, aFormattedNames, aBuf);
			if(Last)
			{
				Line++;
				break;
			}
		}
		str_copy(paMessages[Line], "---------------------------------", sizeof(paMessages[Line]));
	}
	else
	{
		if(pData->m_Offset == 0)
			str_format(paMessages[0], sizeof(paMessages[0]), "%s has no team ranks", pData->m_aName);
		else
			str_format(paMessages[0], sizeof(paMessages[0]), "%s has no team ranks in the specified range", pData->m_aName);
	}
	return false;
}

bool CScoreWorker::ShowTimes(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "DESC" : "ASC";

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("Timestamp", aTimestamp, sizeof(aTimestamp));
	char aBuf[512];
	if(pData->m_aName[0] != '\0') // last 5 times of a player
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Time, (%s-%s) as Ago, %s as Stamp, Server "
			"FROM %s_race "
			"WHERE Map = ? AND Name = ? "
			"ORDER BY Timestamp %s "
			"LIMIT ?, 5",
			aCurrentTimestamp, aTimestamp, aTimestamp,
			pSqlServer->GetPrefix(), pOrder);
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aMap);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindInt(3, LimitStart);
	}
	else // last 5 times of server
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Time, (%s-%s) as Ago, %s as Stamp, Server, Name "
			"FROM %s_race "
			"WHERE Map = ? "
			"ORDER BY Timestamp %s "
			"LIMIT ?, 5",
			aCurrentTimestamp, aTimestamp, aTimestamp,
			pSqlServer->GetPrefix(), pOrder);
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aMap);
		pSqlServer->BindInt(2, LimitStart);
	}

	// show top5
	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(End)
	{
		str_copy(paMessages[0], "There are no times in the specified range", sizeof(paMessages[0]));
		return false;
	}

	str_copy(paMessages[0], "------------- Last Times -------------", sizeof(paMessages[0]));
	int Line = 1;

	do
	{
		float Time = pSqlServer->GetFloat(1);
		str_time_float(Time, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
		int Ago = pSqlServer->GetInt(2);
		int Stamp = pSqlServer->GetInt(3);
		char aServer[5];
		pSqlServer->GetString(4, aServer, sizeof(aServer));
		char aServerFormatted[8] = "\0";
		if(str_comp(aServer, "UNK") != 0)
			str_format(aServerFormatted, sizeof(aServerFormatted), "[%s] ", aServer);

		char aAgoString[40] = "\0";
		sqlstr::AgoTimeToString(Ago, aAgoString, sizeof(aAgoString));

		if(pData->m_aName[0] != '\0') // last 5 times of a player
		{
			if(Stamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s%s, don't know how long ago", aServerFormatted, aBuf);
			else
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s%s ago, %s", aServerFormatted, aAgoString, aBuf);
		}
		else // last 5 times of the server
		{
			char aName[MAX_NAME_LENGTH];
			pSqlServer->GetString(5, aName, sizeof(aName));
			if(Stamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
			{
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s%s, %s, don't know when", aServerFormatted, aName, aBuf);
			}
			else
			{
				str_format(paMessages[Line], sizeof(paMessages[Line]),
					"%s%s, %s ago, %s", aServerFormatted, aName, aAgoString, aBuf);
			}
		}
		Line++;
	} while(!pSqlServer->Step(&End, pError, ErrorSize) && !End);
	if(!End)
	{
		return true;
	}
	str_copy(paMessages[Line], "-------------------------------------------", sizeof(paMessages[Line]));

	return false;
}

bool CScoreWorker::ShowPoints(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT ("
		"  SELECT COUNT(Name) + 1 FROM %s_points WHERE Points > ("
		"    SELECT Points FROM %s_points WHERE Name = ?"
		")) as Ranking, Points, Name "
		"FROM %s_points WHERE Name = ?",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aName);
	pSqlServer->BindString(2, pData->m_aName);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		int Rank = pSqlServer->GetInt(1);
		int Count = pSqlServer->GetInt(2);
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(3, aName, sizeof(aName));
		pResult->m_MessageKind = CScorePlayerResult::ALL;
		str_format(paMessages[0], sizeof(paMessages[0]),
			"%d. %s Points: %d, requested by %s",
			Rank, aName, Count, pData->m_aRequestingPlayer);
	}
	else
	{
		str_format(paMessages[0], sizeof(paMessages[0]),
			"%s has not collected any points so far", pData->m_aName);
	}
	return false;
}

bool CScoreWorker::ShowTopPoints(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int LimitStart = maximum(pData->m_Offset - 1, 0);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT RANK() OVER (ORDER BY a.Points DESC) as Ranking, Points, Name "
		"FROM ("
		"  SELECT Points, Name "
		"  FROM %s_points "
		"  ORDER BY Points DESC LIMIT ?"
		") as a "
		"ORDER BY Ranking ASC, Name ASC LIMIT ?, 5",
		pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindInt(1, LimitStart + 5);
	pSqlServer->BindInt(2, LimitStart);

	// show top points
	str_copy(paMessages[0], "-------- Top Points --------", sizeof(paMessages[0]));

	bool End = false;
	int Line = 1;
	while(!pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		int Rank = pSqlServer->GetInt(1);
		int Points = pSqlServer->GetInt(2);
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(3, aName, sizeof(aName));
		str_format(paMessages[Line], sizeof(paMessages[Line]),
			"%d. %s Points: %d", Rank, aName, Points);
		Line++;
	}
	if(!End)
	{
		return true;
	}
	str_copy(paMessages[Line], "-------------------------------", sizeof(paMessages[Line]));

	return false;
}

bool CScoreWorker::RandomMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreRandomMapResult *>(pGameData->m_pResult.get());

	char aBuf[512];
	if(0 <= pData->m_Stars && pData->m_Stars <= 5)
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Map FROM %s_maps "
			"WHERE Server = ? AND Map != ? AND Stars = ? "
			"ORDER BY %s LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->Random());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindInt(3, pData->m_Stars);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT Map FROM %s_maps "
			"WHERE Server = ? AND Map != ? "
			"ORDER BY %s LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->Random());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
	}
	pSqlServer->BindString(1, pData->m_aServerType);
	pSqlServer->BindString(2, pData->m_aCurrentMap);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		pSqlServer->GetString(1, pResult->m_aMap, sizeof(pResult->m_aMap));
	}
	else
	{
		str_copy(pResult->m_aMessage, "No maps found on this server!", sizeof(pResult->m_aMessage));
	}
	return false;
}

bool CScoreWorker::RandomUnfinishedMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreRandomMapResult *>(pGameData->m_pResult.get());

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
			") ORDER BY %s "
			"LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Random());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aServerType);
		pSqlServer->BindString(2, pData->m_aCurrentMap);
		pSqlServer->BindInt(3, pData->m_Stars);
		pSqlServer->BindString(4, pData->m_aRequestingPlayer);
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
			") ORDER BY %s "
			"LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Random());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aServerType);
		pSqlServer->BindString(2, pData->m_aCurrentMap);
		pSqlServer->BindString(3, pData->m_aRequestingPlayer);
	}

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		pSqlServer->GetString(1, pResult->m_aMap, sizeof(pResult->m_aMap));
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%s has no more unfinished maps on this server!", pData->m_aRequestingPlayer);
		str_copy(pResult->m_aMessage, aBuf, sizeof(pResult->m_aMessage));
	}
	return false;
}

bool CScoreWorker::SaveTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlTeamSaveData *>(pGameData);
	auto *pResult = dynamic_cast<CScoreSaveResult *>(pGameData->m_pResult.get());

	if(w == Write::NORMAL_SUCCEEDED)
	{
		// write succeeded on mysql server. delete from sqlite again
		char aBuf[128] = {0};
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_saves_backup WHERE Code = ?",
			pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		bool End;
		return pSqlServer->Step(&End, pError, ErrorSize);
	}
	if(w == Write::NORMAL_FAILED)
	{
		char aBuf[256] = {0};
		bool End;
		// move to non-tmp table succeeded. delete from backup again
		str_format(aBuf, sizeof(aBuf),
			"INSERT INTO %s_saves SELECT * FROM %s_saves_backup WHERE Code = ?",
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		if(pSqlServer->Step(&End, pError, ErrorSize))
		{
			return true;
		}

		// move to non-tmp table succeeded. delete from backup again
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_saves_backup WHERE Code = ?",
			pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		return pSqlServer->Step(&End, pError, ErrorSize);
	}

	char aSaveId[UUID_MAXSTRSIZE];
	FormatUuid(pResult->m_SaveId, aSaveId, UUID_MAXSTRSIZE);

	char *pSaveState = pResult->m_SavedTeam.GetString();
	char aBuf[65536];

	dbg_msg("score/dbg", "code=%s failure=%d", pData->m_aCode, (int)w);
	bool UseGeneratedCode = pData->m_aCode[0] == '\0' || w != Write::NORMAL;

	bool Retry = false;
	// two tries, first use the user provided code, then the autogenerated
	do
	{
		Retry = false;
		char aCode[128] = {0};
		if(UseGeneratedCode)
			str_copy(aCode, pData->m_aGeneratedCode, sizeof(aCode));
		else
			str_copy(aCode, pData->m_aCode, sizeof(aCode));

		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_saves%s(Savegame, Map, Code, Timestamp, Server, SaveId, DDNet7) "
			"VALUES (?, ?, ?, CURRENT_TIMESTAMP, ?, ?, %s)",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
			w == Write::NORMAL ? "" : "_backup", pSqlServer->False());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pSaveState);
		pSqlServer->BindString(2, pData->m_aMap);
		pSqlServer->BindString(3, aCode);
		pSqlServer->BindString(4, pData->m_aServer);
		pSqlServer->BindString(5, aSaveId);
		pSqlServer->Print();
		int NumInserted;
		if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return true;
		}
		if(NumInserted == 1)
		{
			if(w == Write::NORMAL)
			{
				pResult->m_aBroadcast[0] = '\0';
				if(str_comp(pData->m_aServer, g_Config.m_SvSqlServerName) == 0)
				{
					str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage),
						"Team successfully saved by %s. Use '/load %s' to continue",
						pData->m_aClientName, aCode);
				}
				else
				{
					str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage),
						"Team successfully saved by %s. Use '/load %s' on %s to continue",
						pData->m_aClientName, aCode, pData->m_aServer);
				}
			}
			else
			{
				str_copy(pResult->m_aBroadcast,
					"Database connection failed, teamsave written to a file instead. Admins will add it manually in a few days.",
					sizeof(pResult->m_aBroadcast));
				if(str_comp(pData->m_aServer, g_Config.m_SvSqlServerName) == 0)
				{
					str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage),
						"Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' to continue",
						pData->m_aClientName, aCode);
				}
				else
				{
					str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage),
						"Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' on %s to continue",
						pData->m_aClientName, aCode, pData->m_aServer);
				}
			}

			pResult->m_Status = CScoreSaveResult::SAVE_SUCCESS;
		}
		else if(!UseGeneratedCode)
		{
			UseGeneratedCode = true;
			Retry = true;
		}
	} while(Retry);

	if(pResult->m_Status != CScoreSaveResult::SAVE_SUCCESS)
	{
		dbg_msg("sql", "ERROR: This save-code already exists");
		pResult->m_Status = CScoreSaveResult::SAVE_FAILED;
		str_copy(pResult->m_aMessage, "This save-code already exists", sizeof(pResult->m_aMessage));
	}
	return false;
}

bool CScoreWorker::LoadTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	if(w == Write::NORMAL_SUCCEEDED || w == Write::BACKUP_FIRST)
		return false;
	const auto *pData = dynamic_cast<const CSqlTeamLoadRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreSaveResult *>(pGameData->m_pResult.get());
	pResult->m_Status = CScoreSaveResult::LOAD_FAILED;

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("Timestamp", aTimestamp, sizeof(aTimestamp));

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT Savegame, %s-%s AS Ago, SaveId "
		"FROM %s_saves "
		"where Code = ? AND Map = ? AND DDNet7 = %s",
		aCurrentTimestamp, aTimestamp,
		pSqlServer->GetPrefix(), pSqlServer->False());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aCode);
	pSqlServer->BindString(2, pData->m_aMap);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(End)
	{
		str_copy(pResult->m_aMessage, "No such savegame for this map", sizeof(pResult->m_aMessage));
		return false;
	}

	pResult->m_SaveId = UUID_NO_SAVE_ID;
	if(!pSqlServer->IsNull(3))
	{
		char aSaveId[UUID_MAXSTRSIZE];
		pSqlServer->GetString(3, aSaveId, sizeof(aSaveId));
		if(ParseUuid(&pResult->m_SaveId, aSaveId) || pResult->m_SaveId == UUID_NO_SAVE_ID)
		{
			str_copy(pResult->m_aMessage, "Unable to load savegame: SaveId corrupted", sizeof(pResult->m_aMessage));
			return false;
		}
	}

	char aSaveString[65536];
	pSqlServer->GetString(1, aSaveString, sizeof(aSaveString));
	int Num = pResult->m_SavedTeam.FromString(aSaveString);

	if(Num != 0)
	{
		str_copy(pResult->m_aMessage, "Unable to load savegame: data corrupted", sizeof(pResult->m_aMessage));
		return false;
	}

	bool Found = false;
	for(int i = 0; i < pResult->m_SavedTeam.GetMembersCount(); i++)
	{
		if(str_comp(pResult->m_SavedTeam.m_pSavedTees[i].GetName(), pData->m_aRequestingPlayer) == 0)
		{
			Found = true;
			break;
		}
	}
	if(!Found)
	{
		str_copy(pResult->m_aMessage, "This save exists, but you are not part of it. "
					      "Make sure you use the same name as you had when saving. "
					      "If you saved with an already used code, you get a new random save code, "
					      "check ddnet-saves.txt in config_directory.",
			sizeof(pResult->m_aMessage));
		return false;
	}

	int Since = pSqlServer->GetInt(2);
	if(Since < g_Config.m_SvSaveSwapGamesDelay)
	{
		str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage),
			"You have to wait %d seconds until you can load this savegame",
			g_Config.m_SvSaveSwapGamesDelay - Since);
		return false;
	}

	bool CanLoad = pResult->m_SavedTeam.MatchPlayers(
		pData->m_aClientNames, pData->m_aClientId, pData->m_NumPlayer,
		pResult->m_aMessage, sizeof(pResult->m_aMessage));

	if(!CanLoad)
		return false;

	str_format(aBuf, sizeof(aBuf),
		"DELETE FROM %s_saves "
		"WHERE Code = ? AND Map = ? AND SaveId %s",
		pSqlServer->GetPrefix(),
		pResult->m_SaveId != UUID_NO_SAVE_ID ? "= ?" : "IS NULL");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aCode);
	pSqlServer->BindString(2, pData->m_aMap);
	char aUuid[UUID_MAXSTRSIZE];
	if(pResult->m_SaveId != UUID_NO_SAVE_ID)
	{
		FormatUuid(pResult->m_SaveId, aUuid, sizeof(aUuid));
		pSqlServer->BindString(3, aUuid);
	}
	pSqlServer->Print();
	int NumDeleted;
	if(pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize))
	{
		return true;
	}

	if(NumDeleted != 1)
	{
		str_copy(pResult->m_aMessage, "Unable to load savegame: loaded on a different server", sizeof(pResult->m_aMessage));
		return false;
	}

	pResult->m_Status = CScoreSaveResult::LOAD_SUCCESS;
	str_copy(pResult->m_aMessage, "Loading successfully done", sizeof(pResult->m_aMessage));
	return false;
}

bool CScoreWorker::GetSaves(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aSaveLike[128] = "";
	str_append(aSaveLike, "%\n");
	sqlstr::EscapeLike(aSaveLike + str_length(aSaveLike),
		pData->m_aRequestingPlayer,
		sizeof(aSaveLike) - str_length(aSaveLike));
	str_append(aSaveLike, "\t%");

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aMaxTimestamp[512];
	pSqlServer->ToUnixTimestamp("MAX(Timestamp)", aMaxTimestamp, sizeof(aMaxTimestamp));

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT COUNT(*) AS NumSaves, %s-%s AS Ago "
		"FROM %s_saves "
		"WHERE Map = ? AND Savegame LIKE ?",
		aCurrentTimestamp, aMaxTimestamp,
		pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aMap);
	pSqlServer->BindString(2, aSaveLike);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		int NumSaves = pSqlServer->GetInt(1);
		char aLastSavedString[60] = "\0";
		if(!pSqlServer->IsNull(2))
		{
			int Ago = pSqlServer->GetInt(2);
			char aAgoString[40] = "\0";
			sqlstr::AgoTimeToString(Ago, aAgoString, sizeof(aAgoString));
			str_format(aLastSavedString, sizeof(aLastSavedString), ", last saved %s ago", aAgoString);
		}

		str_format(paMessages[0], sizeof(paMessages[0]),
			"%s has %d save%s on %s%s",
			pData->m_aRequestingPlayer,
			NumSaves, NumSaves == 1 ? "" : "s",
			pData->m_aMap, aLastSavedString);
	}
	return false;
}
