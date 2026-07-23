// Schema v2 implementations of the score worker, see FormatCreateV2* in
// engine/server/databases/connection.cpp for the table layout. Map and player
// names are resolved to integer ids, times are integer centiseconds, finishes
// are recorded in the append-only finish log and the denormalized best table
// answers rank/top queries without aggregating over the whole finish history.
#include "scoreworker.h"

#include <base/dbg.h>
#include <base/hash.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/sql_string_helpers.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// "6b407e81-8b77-3e04-a207-8da17f37d000"
// "save-no-save-id@ddnet.tw"
static const CUuid UUID_NO_SAVE_ID =
	{{0x6b, 0x40, 0x7e, 0x81, 0x8b, 0x77, 0x3e, 0x04,
		0xa2, 0x07, 0x8d, 0xa1, 0x7f, 0x37, 0xd0, 0x00}};

namespace
{

	enum
	{
		// checkpoint times are stored as 25 little-endian int32 centisecond values
		CP_TIMES_BLOB_SIZE = NUM_CHECKPOINTS * 4,
	};

	int TimeToCs(float Time)
	{
		return (int)std::llround((double)Time * 100.0);
	}

	// returns false if the run has no checkpoint times at all, in which case
	// NULL should be stored instead of the blob
	bool PackCpTimes(const float *pTimeCp, unsigned char *pBuf)
	{
		bool HasCp = false;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
		{
			const int Cs = TimeToCs(pTimeCp[i]);
			HasCp = HasCp || Cs != 0;
			pBuf[i * 4] = Cs & 0xff;
			pBuf[i * 4 + 1] = (Cs >> 8) & 0xff;
			pBuf[i * 4 + 2] = (Cs >> 16) & 0xff;
			pBuf[i * 4 + 3] = (Cs >> 24) & 0xff;
		}
		return HasCp;
	}

	void UnpackCpTimes(const unsigned char *pBuf, int Size, float *pTimeCp)
	{
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
		{
			int Cs = 0;
			if((i + 1) * 4 <= Size)
			{
				Cs = pBuf[i * 4] | (pBuf[i * 4 + 1] << 8) | (pBuf[i * 4 + 2] << 16) | ((int)pBuf[i * 4 + 3] << 24);
			}
			pTimeCp[i] = Cs / 100.0f;
		}
	}

	// looks up the id of a map, 0 if it doesn't exist (ids start at 1)
	bool GetMapId(IDbConnection *pSqlServer, const char *pMap, int *pMapId, char *pError, int ErrorSize)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf),
			"SELECT map_id FROM %s_map WHERE name = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pMap);
		bool End;
		if(!pSqlServer->Step(&End, pError, ErrorSize))
		{
			return false;
		}
		*pMapId = End ? 0 : pSqlServer->GetInt(1);
		return true;
	}

	// looks up the id of a player, 0 if it doesn't exist (ids start at 1)
	bool GetPlayerId(IDbConnection *pSqlServer, const char *pPlayer, int *pPlayerId, char *pError, int ErrorSize)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf),
			"SELECT player_id FROM %s_player WHERE name = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pPlayer);
		bool End;
		if(!pSqlServer->Step(&End, pError, ErrorSize))
		{
			return false;
		}
		*pPlayerId = End ? 0 : pSqlServer->GetInt(1);
		return true;
	}

	bool EnsurePlayer(IDbConnection *pSqlServer, const char *pPlayer, int *pPlayerId, char *pError, int ErrorSize)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_player(name) VALUES (%s)%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pPlayer);
		int NumInserted;
		if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return false;
		}
		return GetPlayerId(pSqlServer, pPlayer, pPlayerId, pError, ErrorSize);
	}

	// get-or-create a map row. *pExisted tells whether the map was registered
	// before this call, so that points are only awarded for maps registered by
	// the maps tooling (like v1, which awards no points without a maps row).
	bool EnsureMap(IDbConnection *pSqlServer, const char *pMap, int *pMapId, bool *pExisted, int *pPoints, char *pError, int ErrorSize)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf),
			"SELECT map_id, points FROM %s_map WHERE name = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pMap);
		bool End;
		if(!pSqlServer->Step(&End, pError, ErrorSize))
		{
			return false;
		}
		if(!End)
		{
			*pMapId = pSqlServer->GetInt(1);
			*pExisted = true;
			*pPoints = pSqlServer->GetInt(2);
			return true;
		}
		*pExisted = false;
		*pPoints = 0;
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_map(name) VALUES (%s)%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pMap);
		int NumInserted;
		if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return false;
		}
		return GetMapId(pSqlServer, pMap, pMapId, pError, ErrorSize);
	}

	// Hash identifying a team roster, md5 over the sorted, NUL-separated names.
	// Based on names instead of player ids so that it's stable across databases
	// (the sqlite backup file assigns different player ids than the main server).
	MD5_DIGEST RosterHash(const std::vector<std::string> &vSortedNames)
	{
		char aBuf[MAX_CLIENTS * MAX_NAME_LENGTH] = {0};
		int Length = 0;
		for(const auto &Name : vSortedNames)
		{
			const int NameLength = Name.length();
			mem_copy(aBuf + Length, Name.c_str(), NameLength + 1); // including the NUL separator
			Length += NameLength + 1;
		}
		return md5(aBuf, Length);
	}

} // namespace

bool CScoreWorkerV2::LoadBestTime(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlLoadBestTimeRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreLoadBestTimeResult *>(pGameData->m_pResult.get());

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT time_cs / 100.0 "
		"FROM %s_best "
		"WHERE map_id = (SELECT map_id FROM %s_map WHERE name = %s) "
		"ORDER BY time_cs ASC LIMIT 1",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindString(1, pData->m_aMap);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		pResult->m_CurrentRecord = pSqlServer->GetFloat(1);
	}

	return true;
}

bool CScoreWorkerV2::LoadPlayerData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	pResult->SetVariant(CScorePlayerResult::PLAYER_INFO);

	const char *pPlayer = pData->m_aName[0] != '\0' ? pData->m_aName : pData->m_aRequestingPlayer;
	int MapId, RequestingPlayerId, PlayerId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize) ||
		!GetPlayerId(pSqlServer, pData->m_aRequestingPlayer, &RequestingPlayerId, pError, ErrorSize) ||
		!GetPlayerId(pSqlServer, pPlayer, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aBuf[1024];
	// get best race time of the requesting player and the best checkpoint
	// times of the requested player
	str_format(aBuf, sizeof(aBuf),
		"SELECT"
		"  (SELECT MIN(time_cs) / 100.0 FROM %s_best WHERE map_id = %s AND player_id = %s) AS mintime, "
		"  (SELECT cp_times FROM %s_best WHERE map_id = %s AND player_id = %s AND cp_times IS NOT NULL"
		"   ORDER BY cp_time_cs ASC LIMIT 1) AS cps "
		"FROM %s_best "
		"WHERE map_id = %s AND player_id = %s "
		"LIMIT 1",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(3).c_str(), pSqlServer->Placeholder(4).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(5).c_str(), pSqlServer->Placeholder(6).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindInt(2, RequestingPlayerId);
	pSqlServer->BindInt(3, MapId);
	pSqlServer->BindInt(4, PlayerId);
	pSqlServer->BindInt(5, MapId);
	pSqlServer->BindInt(6, PlayerId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		if(!pSqlServer->IsNull(1))
		{
			pResult->m_Data.m_Info.m_Time = pSqlServer->GetFloat(1);
		}
		if(!pSqlServer->IsNull(2))
		{
			unsigned char aCpBuf[CP_TIMES_BLOB_SIZE];
			const int Size = pSqlServer->GetBlob(2, aCpBuf, sizeof(aCpBuf));
			UnpackCpTimes(aCpBuf, Size, pResult->m_Data.m_Info.m_aTimeCp);
		}
	}

	// birthday check
	str_format(aBuf, sizeof(aBuf),
		"SELECT CURRENT_TIMESTAMP AS cur, first_finished AS stamp "
		"FROM %s_player "
		"WHERE name = %s AND first_finished IS NOT NULL",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindString(1, pData->m_aRequestingPlayer);

	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
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
	return true;
}

bool CScoreWorkerV2::LoadPlayerTimeCp(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	const char *pPlayer = pData->m_aName[0] != '\0' ? pData->m_aName : pData->m_aRequestingPlayer;
	int MapId, PlayerId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize) ||
		!GetPlayerId(pSqlServer, pPlayer, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT cp_time_cs / 100.0, cp_times "
		"FROM %s_best "
		"WHERE map_id = %s AND player_id = %s AND cp_times IS NOT NULL "
		"ORDER BY cp_time_cs ASC "
		"LIMIT 1",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindInt(2, PlayerId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		pResult->SetVariant(CScorePlayerResult::PLAYER_TIMECP);
		pResult->m_Data.m_Info.m_Time = pSqlServer->GetFloat(1);
		unsigned char aCpBuf[CP_TIMES_BLOB_SIZE];
		const int Size = pSqlServer->GetBlob(2, aCpBuf, sizeof(aCpBuf));
		UnpackCpTimes(aCpBuf, Size, pResult->m_Data.m_Info.m_aTimeCp);
		str_copy(pResult->m_Data.m_Info.m_aRequestedPlayer, pPlayer);
	}
	else
	{
		pResult->SetVariant(CScorePlayerResult::DIRECT);
		str_format(paMessages[0], sizeof(paMessages[0]), "'%s' has no checkpoint times available", pPlayer);
	}
	return true;
}

bool CScoreWorkerV2::MapVote(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aFuzzyMap[128];
	str_copy(aFuzzyMap, pData->m_aName);
	sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

	char aMapPrefix[128];
	str_copy(aMapPrefix, pData->m_aName);
	str_append(aMapPrefix, "%");

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf),
		"SELECT name, category "
		"FROM %s_map "
		"WHERE name %s "
		"ORDER BY "
		"  CASE WHEN LOWER(name) = LOWER(%s) THEN 0 ELSE 1 END, "
		"  CASE WHEN name LIKE %s THEN 0 ELSE 1 END, "
		"  LENGTH(name), name "
		"LIMIT 1",
		pSqlServer->GetPrefix(), pSqlServer->LikeNocase(1).c_str(),
		pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindString(1, aFuzzyMap);
	pSqlServer->BindString(2, pData->m_aName);
	pSqlServer->BindString(3, aMapPrefix);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		pResult->SetVariant(CScorePlayerResult::MAP_VOTE);
		auto *pMapVote = &pResult->m_Data.m_MapVote;
		pSqlServer->GetString(1, pMapVote->m_aMap, sizeof(pMapVote->m_aMap));
		pSqlServer->GetString(2, pMapVote->m_aServer, sizeof(pMapVote->m_aServer));
		str_copy(pMapVote->m_aReason, "/map");

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
	return true;
}

bool CScoreWorkerV2::MapInfo(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	int PlayerId;
	if(!GetPlayerId(pSqlServer, pData->m_aRequestingPlayer, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aFuzzyMap[128];
	str_copy(aFuzzyMap, pData->m_aName);
	sqlstr::FuzzyString(aFuzzyMap, sizeof(aFuzzyMap));

	char aMapPrefix[128];
	str_copy(aMapPrefix, pData->m_aName);
	str_append(aMapPrefix, "%");

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("l.released", aTimestamp, sizeof(aTimestamp));

	char aMedianMapTime[2048];
	char aBuf[4096];
	str_format(aBuf, sizeof(aBuf),
		"SELECT l.name, l.category, l.mapper, l.points, l.stars, "
		"  (SELECT COALESCE(SUM(finish_count), 0) FROM %s_best WHERE map_id = l.map_id) AS finishes, "
		"  (SELECT COUNT(DISTINCT player_id) FROM %s_best WHERE map_id = l.map_id) AS finishers, "
		"  (%s) AS median, "
		"  CASE WHEN l.released IS NULL THEN 0 ELSE %s END AS stamp, "
		"  CASE WHEN l.released IS NULL THEN 0 ELSE %s - %s END AS ago, "
		"  (SELECT MIN(time_cs) / 100.0 FROM %s_best WHERE map_id = l.map_id AND player_id = %s) AS owntime "
		"FROM ("
		"  SELECT * FROM %s_map "
		"  WHERE name %s "
		"  ORDER BY "
		"    CASE WHEN LOWER(name) = LOWER(%s) THEN 0 ELSE 1 END, "
		"    CASE WHEN name LIKE %s THEN 0 ELSE 1 END, "
		"    LENGTH(name), "
		"    name "
		"  LIMIT 1"
		") as l",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
		pSqlServer->MedianMapTimeV2(aMedianMapTime, sizeof(aMedianMapTime)),
		aTimestamp, aCurrentTimestamp, aTimestamp,
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->LikeNocase(2).c_str(),
		pSqlServer->Placeholder(3).c_str(), pSqlServer->Placeholder(4).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, PlayerId);
	pSqlServer->BindString(2, aFuzzyMap);
	pSqlServer->BindString(3, pData->m_aName);
	pSqlServer->BindString(4, aMapPrefix);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
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
		float Median = pSqlServer->GetOptionalFloat(8).value_or(-1.0f);
		int Stamp = pSqlServer->GetInt(9);
		int Ago = pSqlServer->GetInt(10);
		float OwnTime = pSqlServer->GetOptionalFloat(11).value_or(-1.0f);

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
			str_time((int64_t)Median * 100, ETimeFormat::HOURS, aBuf, sizeof(aBuf));
			str_format(aMedianString, sizeof(aMedianString), " in %s median", aBuf);
		}

		char aStars[20];
		switch(Stars)
		{
		case 0: str_copy(aStars, "✰✰✰✰✰"); break;
		case 1: str_copy(aStars, "★✰✰✰✰"); break;
		case 2: str_copy(aStars, "★★✰✰✰"); break;
		case 3: str_copy(aStars, "★★★✰✰"); break;
		case 4: str_copy(aStars, "★★★★✰"); break;
		case 5: str_copy(aStars, "★★★★★"); break;
		default: aStars[0] = '\0';
		}

		char aOwnFinishesString[40] = "\0";
		if(OwnTime > 0)
		{
			str_time_float(OwnTime, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));
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
	return true;
}

bool CScoreWorkerV2::SaveScore(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlScoreData *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	char aBuf[1024];

	if(w == Write::NORMAL_SUCCEEDED)
	{
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_finish_backup "
			"WHERE game_uuid = %s AND player_id = (SELECT player_id FROM %s_player WHERE name = %s) AND finished_at = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(2).c_str(), pSqlServer->InsertTimestampAsUtc(3).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aGameUuid);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		pSqlServer->Print();
		int NumDeleted;
		if(!pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize))
		{
			return false;
		}
		if(NumDeleted == 0)
		{
			log_warn("sql", "Rank got moved out of backup database, will show up as duplicate rank in MySQL");
		}
		return true;
	}
	if(w == Write::NORMAL_FAILED)
	{
		// write to the non-backup table succeeded, move the entry from the
		// backup table into the main table of the backup database
		int NumUpdated;
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_finish SELECT * FROM %s_finish_backup "
			"WHERE game_uuid = %s AND player_id = (SELECT player_id FROM %s_player WHERE name = %s) AND finished_at = %s%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
			pSqlServer->Placeholder(1).c_str(),
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(2).c_str(), pSqlServer->InsertTimestampAsUtc(3).c_str(),
			pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aGameUuid);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		pSqlServer->Print();
		if(!pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		{
			return false;
		}

		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_finish_backup "
			"WHERE game_uuid = %s AND player_id = (SELECT player_id FROM %s_player WHERE name = %s) AND finished_at = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(2).c_str(), pSqlServer->InsertTimestampAsUtc(3).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aGameUuid);
		pSqlServer->BindString(2, pData->m_aName);
		pSqlServer->BindString(3, pData->m_aTimestamp);
		pSqlServer->Print();
		if(!pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		{
			return false;
		}
		if(NumUpdated == 0)
		{
			log_warn("sql", "Rank got moved out of backup database, will show up as duplicate rank in MySQL");
		}
		return true;
	}

	if(!pSqlServer->BeginTransaction(pError, ErrorSize))
	{
		return false;
	}

	int PlayerId;
	if(!EnsurePlayer(pSqlServer, pData->m_aName, &PlayerId, pError, ErrorSize))
	{
		return false;
	}
	int MapId, MapPoints;
	bool MapExisted;
	if(!EnsureMap(pSqlServer, pData->m_aMap, &MapId, &MapExisted, &MapPoints, pError, ErrorSize))
	{
		return false;
	}

	if(w == Write::NORMAL)
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT COUNT(*) AS num_finished FROM %s_finish WHERE map_id = %s AND player_id = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindInt(1, MapId);
		pSqlServer->BindInt(2, PlayerId);

		bool End;
		if(!pSqlServer->Step(&End, pError, ErrorSize))
		{
			return false;
		}
		int NumFinished = pSqlServer->GetInt(1);
		if(NumFinished == 0 && MapExisted)
		{
			if(!pSqlServer->AddPoints(pData->m_aName, MapPoints, pError, ErrorSize))
			{
				return false;
			}
			str_format(paMessages[0], sizeof(paMessages[0]),
				"You earned %d point%s for finishing this map!",
				MapPoints, MapPoints == 1 ? "" : "s");
		}
	}

	// remember when the player finished for the first time, for the birthday check
	str_format(aBuf, sizeof(aBuf),
		"UPDATE %s_player SET first_finished = %s WHERE player_id = %s AND first_finished IS NULL",
		pSqlServer->GetPrefix(), pSqlServer->InsertTimestampAsUtc(1).c_str(), pSqlServer->Placeholder(2).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindString(1, pData->m_aTimestamp);
	pSqlServer->BindInt(2, PlayerId);
	int NumUpdated;
	if(!pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
	{
		return false;
	}

	unsigned char aCpBuf[CP_TIMES_BLOB_SIZE];
	const bool HasCp = PackCpTimes(pData->m_aCurrentTimeCp, aCpBuf);
	const int TimeCs = TimeToCs(pData->m_Time);

	// record the finish in the append-only log
	str_format(aBuf, sizeof(aBuf),
		"%s INTO %s_finish%s(map_id, player_id, time_cs, finished_at, server, game_uuid, cp_times, ddnet7) "
		"VALUES (%s, %s, %s, %s, %s, %s, %s, %s)%s",
		pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
		w == Write::NORMAL ? "" : "_backup",
		pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(),
		pSqlServer->InsertTimestampAsUtc(4).c_str(), pSqlServer->Placeholder(5).c_str(), pSqlServer->Placeholder(6).c_str(),
		pSqlServer->Placeholder(7).c_str(), pSqlServer->False(), pSqlServer->InsertIgnoreEnd());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindInt(2, PlayerId);
	pSqlServer->BindInt(3, TimeCs);
	pSqlServer->BindString(4, pData->m_aTimestamp);
	pSqlServer->BindString(5, g_Config.m_SvSqlServerName);
	pSqlServer->BindString(6, pData->m_aGameUuid);
	if(HasCp)
		pSqlServer->BindBlob(7, aCpBuf, sizeof(aCpBuf));
	else
		pSqlServer->BindNull(7);
	pSqlServer->Print();
	int NumInserted;
	if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
	{
		return false;
	}

	if(w == Write::NORMAL)
	{
		// keep the denormalized best table up to date
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_best(map_id, player_id, server, time_cs, finish_count, first_finished, last_finished) "
			"VALUES (%s, %s, %s, %s, 0, %s, %s)%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
			pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(),
			pSqlServer->Placeholder(4).c_str(), pSqlServer->InsertTimestampAsUtc(5).c_str(), pSqlServer->InsertTimestampAsUtc(6).c_str(),
			pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindInt(1, MapId);
		pSqlServer->BindInt(2, PlayerId);
		pSqlServer->BindString(3, g_Config.m_SvSqlServerName);
		pSqlServer->BindInt(4, TimeCs);
		pSqlServer->BindString(5, pData->m_aTimestamp);
		pSqlServer->BindString(6, pData->m_aTimestamp);
		if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return false;
		}

		// The order of the SET expressions matters for MySQL, which evaluates
		// them left to right: cp_times has to be assigned before cp_time_cs,
		// which its condition reads.
		str_format(aBuf, sizeof(aBuf),
			"UPDATE %s_best SET "
			"  finish_count = finish_count + 1, "
			"  last_finished = %s, "
			"  cp_times = CASE WHEN %s = 1 AND (cp_time_cs IS NULL OR cp_time_cs > %s) THEN %s ELSE cp_times END, "
			"  cp_time_cs = CASE WHEN %s = 1 AND (cp_time_cs IS NULL OR cp_time_cs > %s) THEN %s ELSE cp_time_cs END, "
			"  time_cs = CASE WHEN time_cs > %s THEN %s ELSE time_cs END "
			"WHERE map_id = %s AND player_id = %s AND server = %s",
			pSqlServer->GetPrefix(), pSqlServer->InsertTimestampAsUtc(1).c_str(),
			pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(), pSqlServer->Placeholder(4).c_str(),
			pSqlServer->Placeholder(5).c_str(), pSqlServer->Placeholder(6).c_str(), pSqlServer->Placeholder(7).c_str(),
			pSqlServer->Placeholder(8).c_str(), pSqlServer->Placeholder(9).c_str(),
			pSqlServer->Placeholder(10).c_str(), pSqlServer->Placeholder(11).c_str(), pSqlServer->Placeholder(12).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aTimestamp);
		pSqlServer->BindInt(2, HasCp ? 1 : 0);
		pSqlServer->BindInt(3, TimeCs);
		if(HasCp)
			pSqlServer->BindBlob(4, aCpBuf, sizeof(aCpBuf));
		else
			pSqlServer->BindNull(4);
		pSqlServer->BindInt(5, HasCp ? 1 : 0);
		pSqlServer->BindInt(6, TimeCs);
		pSqlServer->BindInt(7, TimeCs);
		pSqlServer->BindInt(8, TimeCs);
		pSqlServer->BindInt(9, TimeCs);
		pSqlServer->BindInt(10, MapId);
		pSqlServer->BindInt(11, PlayerId);
		pSqlServer->BindString(12, g_Config.m_SvSqlServerName);
		if(!pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		{
			return false;
		}
	}

	return pSqlServer->CommitTransaction(pError, ErrorSize);
}

bool CScoreWorkerV2::SaveTeamScore(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlTeamScoreData *>(pGameData);

	char aBuf[1024];

	if(w == Write::NORMAL_SUCCEEDED)
	{
		// copy uuid, because mysql BindBlob doesn't support const buffers
		CUuid TeamrankId = pData->m_TeamrankUuid;
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_team_backup WHERE team_id = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->Print();
		int NumDeleted;
		if(!pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize))
		{
			return false;
		}
		if(NumDeleted == 0)
		{
			log_warn("sql", "Teamrank got moved out of backup database, will show up as duplicate teamrank in MySQL");
		}
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_team_player WHERE team_id = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		return pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize);
	}
	if(w == Write::NORMAL_FAILED)
	{
		int NumInserted;
		CUuid TeamrankId = pData->m_TeamrankUuid;

		// the roster might already have a team rank in the main table, in
		// which case the improvement is lost, like the duplicate teamranks
		// of the v1 flow
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_team SELECT * FROM %s_team_backup WHERE team_id = %s%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
			pSqlServer->Placeholder(1).c_str(), pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->Print();
		if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return false;
		}

		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_team_backup WHERE team_id = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->Print();
		return pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize);
	}

	// get the names sorted in a tab separated string
	std::vector<std::string> vNames;
	vNames.reserve(pData->m_Size);
	for(unsigned int i = 0; i < pData->m_Size; i++)
		vNames.emplace_back(pData->m_aaNames[i]);
	std::sort(vNames.begin(), vNames.end());
	MD5_DIGEST Hash = RosterHash(vNames);

	if(!pSqlServer->BeginTransaction(pError, ErrorSize))
	{
		return false;
	}

	int MapId, MapPoints;
	bool MapExisted;
	if(!EnsureMap(pSqlServer, pData->m_aMap, &MapId, &MapExisted, &MapPoints, pError, ErrorSize))
	{
		return false;
	}
	std::vector<int> vPlayerIds;
	for(unsigned int i = 0; i < pData->m_Size; i++)
	{
		int PlayerId;
		if(!EnsurePlayer(pSqlServer, pData->m_aaNames[i], &PlayerId, pError, ErrorSize))
		{
			return false;
		}
		vPlayerIds.push_back(PlayerId);
	}

	if(w == Write::NORMAL)
	{
		// find a team rank of the same roster
		str_format(aBuf, sizeof(aBuf),
			"SELECT team_id, time_cs / 100.0 "
			"FROM %s_team "
			"WHERE map_id = %s AND roster_hash = %s AND ddnet7 = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
			pSqlServer->False());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindInt(1, MapId);
		pSqlServer->BindBlob(2, Hash.data, sizeof(Hash.data));

		bool End;
		if(!pSqlServer->Step(&End, pError, ErrorSize))
		{
			return false;
		}
		if(!End)
		{
			CUuid TeamId;
			pSqlServer->GetBlob(1, TeamId.m_aData, sizeof(TeamId.m_aData));
			float Time = pSqlServer->GetFloat(2);
			dbg_msg("sql", "found team rank from same team (old time: %f, new time: %f)", Time, pData->m_Time);
			if(pData->m_Time < Time)
			{
				str_format(aBuf, sizeof(aBuf),
					"UPDATE %s_team SET time_cs = %s, finished_at = %s, game_uuid = %s WHERE team_id = %s",
					pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->InsertTimestampAsUtc(2).c_str(),
					pSqlServer->Placeholder(3).c_str(), pSqlServer->Placeholder(4).c_str());
				if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
				{
					return false;
				}
				pSqlServer->BindInt(1, TimeToCs(pData->m_Time));
				pSqlServer->BindString(2, pData->m_aTimestamp);
				pSqlServer->BindString(3, pData->m_aGameUuid);
				pSqlServer->BindBlob(4, TeamId.m_aData, sizeof(TeamId.m_aData));
				pSqlServer->Print();
				int NumUpdated;
				if(!pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
				{
					return false;
				}
				if(!pSqlServer->CommitTransaction(pError, ErrorSize))
				{
					return false;
				}
				// return error if we didn't update any rows
				return NumUpdated != 0;
			}
			return pSqlServer->CommitTransaction(pError, ErrorSize);
		}
	}

	// if no entry found... create a new one
	CUuid TeamrankId = pData->m_TeamrankUuid;
	str_format(aBuf, sizeof(aBuf),
		"%s INTO %s_team%s(team_id, map_id, roster_hash, time_cs, finished_at, server, game_uuid, member_count, ddnet7) "
		"VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)%s",
		pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
		w == Write::NORMAL ? "" : "_backup",
		pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(),
		pSqlServer->Placeholder(4).c_str(), pSqlServer->InsertTimestampAsUtc(5).c_str(), pSqlServer->Placeholder(6).c_str(),
		pSqlServer->Placeholder(7).c_str(), pSqlServer->Placeholder(8).c_str(),
		pSqlServer->False(), pSqlServer->InsertIgnoreEnd());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
	pSqlServer->BindInt(2, MapId);
	pSqlServer->BindBlob(3, Hash.data, sizeof(Hash.data));
	pSqlServer->BindInt(4, TimeToCs(pData->m_Time));
	pSqlServer->BindString(5, pData->m_aTimestamp);
	pSqlServer->BindString(6, g_Config.m_SvSqlServerName);
	pSqlServer->BindString(7, pData->m_aGameUuid);
	pSqlServer->BindInt(8, (int)pData->m_Size);
	pSqlServer->Print();
	int NumInserted;
	if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
	{
		return false;
	}
	for(int PlayerId : vPlayerIds)
	{
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_team_player(team_id, player_id) VALUES (%s, %s)%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
			pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(), pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindBlob(1, TeamrankId.m_aData, sizeof(TeamrankId.m_aData));
		pSqlServer->BindInt(2, PlayerId);
		if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return false;
		}
	}
	return pSqlServer->CommitTransaction(pError, ErrorSize);
}

bool CScoreWorkerV2::ShowRank(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	int MapId, PlayerId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize) ||
		!GetPlayerId(pSqlServer, pData->m_aName, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aServerLike[16];
	str_format(aServerLike, sizeof(aServerLike), "%%%s%%", pData->m_aServer);

	char aBuf[600];
	str_format(aBuf, sizeof(aBuf),
		"SELECT ranking, time, percentrank "
		"FROM ("
		"  SELECT RANK() OVER w AS ranking, PERCENT_RANK() OVER w AS percentrank, MIN(time_cs) / 100.0 AS time, player_id "
		"  FROM %s_best "
		"  WHERE map_id = %s "
		"  AND server LIKE %s "
		"  GROUP BY player_id "
		"  WINDOW w AS (ORDER BY MIN(time_cs))"
		") as a "
		"WHERE player_id = %s",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
		pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str());

	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, aServerLike);
	pSqlServer->BindInt(3, PlayerId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}

	char aRegionalRank[16];
	if(End)
	{
		str_copy(aRegionalRank, "unranked");
	}
	else
	{
		str_format(aRegionalRank, sizeof(aRegionalRank), "rank %d", pSqlServer->GetInt(1));
	}

	const char *pAny = "%";

	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, pAny);
	pSqlServer->BindInt(3, PlayerId);

	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}

	if(!End)
	{
		int Rank = pSqlServer->GetInt(1);
		float Time = pSqlServer->GetFloat(2);
		str_time_float(Time, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));

		if(g_Config.m_SvHideScore)
		{
			str_format(pResult->m_Data.m_aaMessages[0], sizeof(pResult->m_Data.m_aaMessages[0]),
				"Your time: %s", aBuf);
		}
		else
		{
			pResult->m_MessageKind = CScorePlayerResult::ALL;
			// CEIL and FLOOR are not supported in SQLite
			int BetterThanPercent = std::floor(100.0f - 100.0f * pSqlServer->GetFloat(3));

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
	return true;
}

bool CScoreWorkerV2::ShowTeamRank(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	int MapId, PlayerId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize) ||
		!GetPlayerId(pSqlServer, pData->m_aName, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aBuf[2400];
	str_format(aBuf, sizeof(aBuf),
		"SELECT t.team_id, p.name, t.time_cs / 100.0 AS time, teamrank.ranking, teamrank.percentrank "
		"FROM (" // teamrank score board
		"  SELECT RANK() OVER w AS ranking, PERCENT_RANK() OVER w AS percentrank, team_id "
		"  FROM %s_team "
		"  WHERE map_id = %s "
		"  WINDOW w AS (ORDER BY time_cs)"
		") AS teamrank "
		"JOIN (" // best team with the player in it
		"  SELECT t2.team_id AS best_id "
		"  FROM %s_team t2 "
		"  JOIN %s_team_player tp2 ON tp2.team_id = t2.team_id "
		"  WHERE t2.map_id = %s AND tp2.player_id = %s "
		"  ORDER BY t2.time_cs "
		"  LIMIT 1"
		") AS l ON teamrank.team_id = l.best_id "
		"JOIN %s_team t ON t.team_id = l.best_id "
		"JOIN %s_team_player tp ON tp.team_id = t.team_id "
		"JOIN %s_player p ON p.player_id = tp.player_id "
		"ORDER BY p.name",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindInt(2, MapId);
	pSqlServer->BindInt(3, PlayerId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		float Time = pSqlServer->GetFloat(3);
		str_time_float(Time, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));
		int Rank = pSqlServer->GetInt(4);
		// CEIL and FLOOR are not supported in SQLite
		int BetterThanPercent = std::floor(100.0f - 100.0f * pSqlServer->GetFloat(5));
		CTeamrank Teamrank;
		if(!Teamrank.NextSqlResult(pSqlServer, &End, pError, ErrorSize))
		{
			return false;
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
	return true;
}

bool CScoreWorkerV2::ShowTop(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());

	int MapId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize))
	{
		return false;
	}

	int LimitStart = std::max(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";
	const char *pAny = "%";

	char aBuf[512];
	// limit inside the derived table so that only the displayed rows are
	// joined against the player names, the window still ranks the whole map
	str_format(aBuf, sizeof(aBuf),
		"SELECT p.name, a.time, a.ranking "
		"FROM ("
		"  SELECT RANK() OVER w AS ranking, MIN(time_cs) / 100.0 AS time, player_id "
		"  FROM %s_best "
		"  WHERE map_id = %s "
		"  AND server LIKE %s "
		"  GROUP BY player_id "
		"  WINDOW w AS (ORDER BY MIN(time_cs)) "
		"  ORDER BY ranking %s "
		"  LIMIT %s OFFSET %d"
		") as a "
		"JOIN %s_player p ON p.player_id = a.player_id "
		"ORDER BY a.ranking %s",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
		pOrder, pSqlServer->Placeholder(3).c_str(), LimitStart,
		pSqlServer->GetPrefix(), pOrder);

	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, pAny);
	pSqlServer->BindInt(3, 5);

	// show top
	int Line = 0;
	str_copy(pResult->m_Data.m_aaMessages[Line], "------------ Global Top ------------");
	Line++;

	char aTime[32];
	bool End = false;

	while(pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(1, aName, sizeof(aName));
		float Time = pSqlServer->GetFloat(2);
		str_time_float(Time, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
		int Rank = pSqlServer->GetInt(3);
		str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
			"%d. %s Time: %s", Rank, aName, aTime);

		Line++;
	}

	if(!g_Config.m_SvRegionalRankings)
	{
		str_copy(pResult->m_Data.m_aaMessages[Line], "-----------------------------------------");
		return End;
	}

	char aServerLike[16];
	str_format(aServerLike, sizeof(aServerLike), "%%%s%%", pData->m_aServer);

	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, aServerLike);
	pSqlServer->BindInt(3, 3);

	str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
		"------------ %s Top ------------", pData->m_aServer);
	Line++;

	// show top
	while(pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		char aName[MAX_NAME_LENGTH];
		pSqlServer->GetString(1, aName, sizeof(aName));
		float Time = pSqlServer->GetFloat(2);
		str_time_float(Time, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
		int Rank = pSqlServer->GetInt(3);
		str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
			"%d. %s Time: %s", Rank, aName, aTime);
		Line++;
	}

	return End;
}

bool CScoreWorkerV2::ShowTeamTop5(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int MapId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize))
	{
		return false;
	}

	int LimitStart = std::max(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";
	const char *pAny = "%";

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf),
		"SELECT p.name, t.time_cs / 100.0 AS time, l.ranking, t.member_count "
		"FROM ("
		"  SELECT RANK() OVER w AS ranking, team_id "
		"  FROM %s_team "
		"  WHERE map_id = %s "
		"  AND server LIKE %s "
		"  WINDOW w AS (ORDER BY time_cs) "
		"  ORDER BY ranking %s "
		"  LIMIT %s OFFSET %d"
		") as l "
		"JOIN %s_team t ON t.team_id = l.team_id "
		"JOIN %s_team_player tp ON tp.team_id = t.team_id "
		"JOIN %s_player p ON p.player_id = tp.player_id "
		"ORDER BY l.ranking %s, t.team_id, p.name ASC",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
		pOrder, pSqlServer->Placeholder(3).c_str(), LimitStart,
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pOrder);
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, pAny);
	pSqlServer->BindInt(3, 5);

	int Line = 0;
	str_copy(paMessages[Line++], "------- Team Top 5 -------", sizeof(paMessages[Line]));

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		if(!CTeamrank::GetSqlTop5Team(pSqlServer, &End, pError, ErrorSize, paMessages, &Line, 5))
		{
			return false;
		}
	}

	if(!g_Config.m_SvRegionalRankings)
	{
		str_copy(paMessages[Line], "-------------------------------");
		return true;
	}

	char aServerLike[16];
	str_format(aServerLike, sizeof(aServerLike), "%%%s%%", pData->m_aServer);

	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, aServerLike);
	pSqlServer->BindInt(3, 3);

	str_format(pResult->m_Data.m_aaMessages[Line], sizeof(pResult->m_Data.m_aaMessages[Line]),
		"----- %s Team Top -----", pData->m_aServer);
	Line++;

	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		if(!CTeamrank::GetSqlTop5Team(pSqlServer, &End, pError, ErrorSize, paMessages, &Line, 3))
		{
			return false;
		}
	}
	return true;
}

bool CScoreWorkerV2::ShowPlayerTeamTop5(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int MapId, PlayerId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize) ||
		!GetPlayerId(pSqlServer, pData->m_aName, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	int LimitStart = std::max(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "ASC" : "DESC";

	char aBuf[2400];
	str_format(aBuf, sizeof(aBuf),
		"SELECT t.team_id, p.name, t.time_cs / 100.0 AS time, teamrank.ranking "
		"FROM (" // teamrank score board
		"  SELECT RANK() OVER w AS ranking, team_id "
		"  FROM %s_team "
		"  WHERE map_id = %s "
		"  WINDOW w AS (ORDER BY time_cs)"
		") AS teamrank "
		"JOIN (" // select teams with the player in it
		"  SELECT t2.team_id AS player_team_id "
		"  FROM %s_team t2 "
		"  JOIN %s_team_player tp2 ON tp2.team_id = t2.team_id "
		"  WHERE t2.map_id = %s AND tp2.player_id = %s "
		"  ORDER BY t2.time_cs %s "
		"  LIMIT 5 OFFSET %d"
		") AS l ON teamrank.team_id = l.player_team_id "
		"JOIN %s_team t ON t.team_id = l.player_team_id "
		"JOIN %s_team_player tp ON tp.team_id = t.team_id "
		"JOIN %s_player p ON p.player_id = tp.player_id "
		"ORDER BY t.time_cs %s, t.team_id, p.name ASC",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(),
		pOrder, LimitStart,
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pOrder);
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindInt(2, MapId);
	pSqlServer->BindInt(3, PlayerId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		// show teamtop5
		int Line = 0;
		str_copy(paMessages[Line++], "------- Team Top 5 -------", sizeof(paMessages[Line]));

		for(Line = 1; Line < 6; Line++) // print
		{
			float Time = pSqlServer->GetFloat(3);
			str_time_float(Time, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));
			int Rank = pSqlServer->GetInt(4);
			CTeamrank Teamrank;
			bool Last;
			if(!Teamrank.NextSqlResult(pSqlServer, &Last, pError, ErrorSize))
			{
				return false;
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
		str_copy(paMessages[Line], "---------------------------------");
	}
	else
	{
		if(pData->m_Offset == 0)
			str_format(paMessages[0], sizeof(paMessages[0]), "%s has no team ranks", pData->m_aName);
		else
			str_format(paMessages[0], sizeof(paMessages[0]), "%s has no team ranks in the specified range", pData->m_aName);
	}
	return true;
}

bool CScoreWorkerV2::ShowTimes(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int MapId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize))
	{
		return false;
	}

	int LimitStart = std::max(absolute(pData->m_Offset) - 1, 0);
	const char *pOrder = pData->m_Offset >= 0 ? "DESC" : "ASC";

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("f.finished_at", aTimestamp, sizeof(aTimestamp));
	char aBuf[512];
	if(pData->m_aName[0] != '\0') // last 5 times of a player
	{
		int PlayerId;
		if(!GetPlayerId(pSqlServer, pData->m_aName, &PlayerId, pError, ErrorSize))
		{
			return false;
		}
		str_format(aBuf, sizeof(aBuf),
			"SELECT f.time_cs / 100.0, (%s-%s) as ago, %s as stamp, f.server "
			"FROM %s_finish f "
			"WHERE f.map_id = %s AND f.player_id = %s "
			"ORDER BY f.finished_at %s "
			"LIMIT 5 OFFSET %s",
			aCurrentTimestamp, aTimestamp, aTimestamp,
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
			pOrder, pSqlServer->Placeholder(3).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindInt(1, MapId);
		pSqlServer->BindInt(2, PlayerId);
		pSqlServer->BindInt(3, LimitStart);
	}
	else // last 5 times of server
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT f.time_cs / 100.0, (%s-%s) as ago, %s as stamp, f.server, p.name "
			"FROM %s_finish f "
			"JOIN %s_player p ON p.player_id = f.player_id "
			"WHERE f.map_id = %s "
			"ORDER BY f.finished_at %s "
			"LIMIT 5 OFFSET %s",
			aCurrentTimestamp, aTimestamp, aTimestamp,
			pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
			pOrder, pSqlServer->Placeholder(2).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindInt(1, MapId);
		pSqlServer->BindInt(2, LimitStart);
	}

	// show top5
	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(End)
	{
		str_copy(paMessages[0], "There are no times in the specified range");
		return true;
	}

	str_copy(paMessages[0], "------------- Last Times -------------");
	int Line = 1;

	do
	{
		float Time = pSqlServer->GetFloat(1);
		str_time_float(Time, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));
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
	} while(pSqlServer->Step(&End, pError, ErrorSize) && !End);
	if(!End)
	{
		return false;
	}
	str_copy(paMessages[Line], "-------------------------------------------");

	return true;
}

bool CScoreWorkerV2::ShowPoints(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int PlayerId;
	if(!GetPlayerId(pSqlServer, pData->m_aName, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT ("
		"  SELECT COUNT(*) + 1 FROM %s_player_points WHERE points > ("
		"    SELECT points FROM %s_player_points WHERE player_id = %s"
		")) as ranking, pp.points, p.name "
		"FROM %s_player_points pp "
		"JOIN %s_player p ON p.player_id = pp.player_id "
		"WHERE pp.player_id = %s",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(),
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(), pSqlServer->Placeholder(2).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, PlayerId);
	pSqlServer->BindInt(2, PlayerId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
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
	return true;
}

bool CScoreWorkerV2::ShowTopPoints(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int LimitStart = std::max(pData->m_Offset - 1, 0);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT RANK() OVER (ORDER BY a.points DESC) as ranking, points, name "
		"FROM ("
		"  SELECT pp.points, p.name "
		"  FROM %s_player_points pp "
		"  JOIN %s_player p ON p.player_id = pp.player_id "
		"  ORDER BY pp.points DESC LIMIT %s"
		") as a "
		"ORDER BY ranking ASC, name ASC LIMIT 5 OFFSET %s",
		pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
		pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, LimitStart + 5);
	pSqlServer->BindInt(2, LimitStart);

	// show top points
	str_copy(paMessages[0], "-------- Top Points --------");

	bool End = false;
	int Line = 1;
	while(pSqlServer->Step(&End, pError, ErrorSize) && !End)
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
		return false;
	}
	str_copy(paMessages[Line], "-------------------------------");

	return true;
}

bool CScoreWorkerV2::RandomMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreRandomMapResult *>(pGameData->m_pResult.get());

	char aBuf[512];
	if(in_range(pData->m_MinStars, 0, 5) && in_range(pData->m_MaxStars, 0, 5))
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT name FROM %s_map "
			"WHERE category = %s AND name != %s AND stars BETWEEN %s AND %s "
			"ORDER BY %s LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
			pSqlServer->Placeholder(3).c_str(), pSqlServer->Placeholder(4).c_str(), pSqlServer->Random());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindInt(3, pData->m_MinStars);
		pSqlServer->BindInt(4, pData->m_MaxStars);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT name FROM %s_map "
			"WHERE category = %s AND name != %s "
			"ORDER BY %s LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
			pSqlServer->Random());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
	}
	pSqlServer->BindString(1, pData->m_aServerType);
	pSqlServer->BindString(2, pData->m_aCurrentMap);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		pSqlServer->GetString(1, pResult->m_aMap, sizeof(pResult->m_aMap));
	}
	else
	{
		str_copy(pResult->m_aMessage, "No maps found on this server!");
	}
	return true;
}

bool CScoreWorkerV2::RandomUnfinishedMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRandomMapRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreRandomMapResult *>(pGameData->m_pResult.get());

	int PlayerId;
	if(!GetPlayerId(pSqlServer, pData->m_aRequestingPlayer, &PlayerId, pError, ErrorSize))
	{
		return false;
	}

	char aBuf[512];
	if(in_range(pData->m_MinStars, 0, 5) && in_range(pData->m_MaxStars, 0, 5))
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT name "
			"FROM %s_map "
			"WHERE category = %s AND name != %s AND stars BETWEEN %s AND %s AND map_id NOT IN ("
			"  SELECT map_id "
			"  FROM %s_best "
			"  WHERE player_id = %s"
			") ORDER BY %s "
			"LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
			pSqlServer->Placeholder(3).c_str(), pSqlServer->Placeholder(4).c_str(),
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(5).c_str(), pSqlServer->Random());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aServerType);
		pSqlServer->BindString(2, pData->m_aCurrentMap);
		pSqlServer->BindInt(3, pData->m_MinStars);
		pSqlServer->BindInt(4, pData->m_MaxStars);
		pSqlServer->BindInt(5, PlayerId);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT name "
			"FROM %s_map "
			"WHERE category = %s AND name != %s AND map_id NOT IN ("
			"  SELECT map_id "
			"  FROM %s_best "
			"  WHERE player_id = %s"
			") ORDER BY %s "
			"LIMIT 1",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(3).c_str(), pSqlServer->Random());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aServerType);
		pSqlServer->BindString(2, pData->m_aCurrentMap);
		pSqlServer->BindInt(3, PlayerId);
	}

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(!End)
	{
		pSqlServer->GetString(1, pResult->m_aMap, sizeof(pResult->m_aMap));
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%s has no more unfinished maps on this server!", pData->m_aRequestingPlayer);
		str_copy(pResult->m_aMessage, aBuf);
	}
	return true;
}

bool CScoreWorkerV2::SaveTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlTeamSaveData *>(pGameData);
	auto *pResult = dynamic_cast<CScoreSaveResult *>(pGameData->m_pResult.get());

	char aBuf[1024];
	if(w == Write::NORMAL_SUCCEEDED)
	{
		// write succeeded on the remote server, delete from backup again
		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_save_backup WHERE code = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		bool End;
		return pSqlServer->Step(&End, pError, ErrorSize);
	}
	if(w == Write::NORMAL_FAILED)
	{
		bool End;
		// move to non-tmp table succeeded. delete from backup again
		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_save SELECT * FROM %s_save_backup WHERE code = %s%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(), pSqlServer->GetPrefix(),
			pSqlServer->Placeholder(1).c_str(), pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		if(!pSqlServer->Step(&End, pError, ErrorSize))
		{
			return false;
		}

		str_format(aBuf, sizeof(aBuf),
			"DELETE FROM %s_save_backup WHERE code = %s",
			pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pData->m_aGeneratedCode);
		return pSqlServer->Step(&End, pError, ErrorSize);
	}

	char aSaveId[UUID_MAXSTRSIZE];
	FormatUuid(pResult->m_SaveId, aSaveId, UUID_MAXSTRSIZE);

	char *pSaveState = pResult->m_SavedTeam.GetString();

	dbg_msg("score/dbg", "code=%s failure=%d", pData->m_aCode, (int)w);
	bool UseGeneratedCode = pData->m_aCode[0] == '\0' || w != Write::NORMAL;

	str_copy(pResult->m_aGeneratedCode, pData->m_aGeneratedCode);
	str_copy(pResult->m_aCode, UseGeneratedCode ? "" : pData->m_aCode);

	if(!pSqlServer->BeginTransaction(pError, ErrorSize))
	{
		return false;
	}

	int MapId, MapPoints;
	bool MapExisted;
	if(!EnsureMap(pSqlServer, pData->m_aMap, &MapId, &MapExisted, &MapPoints, pError, ErrorSize))
	{
		return false;
	}

	bool Retry = false;
	// two tries, first use the user provided code, then the autogenerated
	do
	{
		Retry = false;
		char aCode[128] = {0};
		if(UseGeneratedCode)
			str_copy(aCode, pData->m_aGeneratedCode);
		else
			str_copy(aCode, pData->m_aCode);

		str_format(aBuf, sizeof(aBuf),
			"%s INTO %s_save%s(savegame, map_id, code, saved_at, server, save_uuid, ddnet7) "
			"VALUES (%s, %s, %s, CURRENT_TIMESTAMP, %s, %s, %s)%s",
			pSqlServer->InsertIgnore(), pSqlServer->GetPrefix(),
			w == Write::NORMAL ? "" : "_backup",
			pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(), pSqlServer->Placeholder(3).c_str(),
			pSqlServer->Placeholder(4).c_str(), pSqlServer->Placeholder(5).c_str(),
			pSqlServer->False(), pSqlServer->InsertIgnoreEnd());
		if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return false;
		}
		pSqlServer->BindString(1, pSaveState);
		pSqlServer->BindInt(2, MapId);
		pSqlServer->BindString(3, aCode);
		pSqlServer->BindString(4, pData->m_aServer);
		pSqlServer->BindString(5, aSaveId);
		pSqlServer->Print();
		int NumInserted;
		if(!pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return false;
		}
		if(NumInserted == 1)
		{
			pResult->m_Status = CScoreSaveResult::SAVE_SUCCESS;
			if(UseGeneratedCode)
			{
				pResult->m_aCode[0] = '\0';
			}
			if(w != Write::NORMAL)
			{
				if(str_comp(pData->m_aServer, g_Config.m_SvSqlServerName) == 0)
				{
					pResult->m_aServer[0] = '\0';
				}
				pResult->m_Status = CScoreSaveResult::SAVE_FALLBACKFILE;
			}
		}
		else if(!UseGeneratedCode)
		{
			UseGeneratedCode = true;
			Retry = true;
		}
	} while(Retry);

	if(!pSqlServer->CommitTransaction(pError, ErrorSize))
	{
		return false;
	}

	if(
		pResult->m_Status != CScoreSaveResult::SAVE_SUCCESS &&
		pResult->m_Status != CScoreSaveResult::SAVE_WARNING &&
		pResult->m_Status != CScoreSaveResult::SAVE_FALLBACKFILE)
	{
		dbg_msg("sql", "ERROR: This save-code already exists");
		pResult->m_Status = CScoreSaveResult::SAVE_FAILED;
		str_copy(pResult->m_aMessage, "This save-code already exists");
	}
	return true;
}

bool CScoreWorkerV2::LoadTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	if(w == Write::NORMAL_SUCCEEDED || w == Write::BACKUP_FIRST)
		return true;
	const auto *pData = dynamic_cast<const CSqlTeamLoadRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScoreSaveResult *>(pGameData->m_pResult.get());
	pResult->m_Status = CScoreSaveResult::LOAD_FAILED;

	int MapId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize))
	{
		return false;
	}

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aTimestamp[512];
	pSqlServer->ToUnixTimestamp("saved_at", aTimestamp, sizeof(aTimestamp));

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT savegame, %s-%s AS ago, save_uuid "
		"FROM %s_save "
		"WHERE code = %s AND map_id = %s AND ddnet7 = %s",
		aCurrentTimestamp, aTimestamp,
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
		pSqlServer->False());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindString(1, pData->m_aCode);
	pSqlServer->BindInt(2, MapId);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
	}
	if(End)
	{
		str_copy(pResult->m_aMessage, "No such savegame for this map");
		return true;
	}

	pResult->m_SaveId = UUID_NO_SAVE_ID;
	if(!pSqlServer->IsNull(3))
	{
		char aSaveId[UUID_MAXSTRSIZE];
		pSqlServer->GetString(3, aSaveId, sizeof(aSaveId));
		if(ParseUuid(&pResult->m_SaveId, aSaveId) || pResult->m_SaveId == UUID_NO_SAVE_ID)
		{
			str_copy(pResult->m_aMessage, "Unable to load savegame: SaveId corrupted");
			return true;
		}
	}

	char aSaveString[65536];
	pSqlServer->GetString(1, aSaveString, sizeof(aSaveString));
	int Num = pResult->m_SavedTeam.FromString(aSaveString);

	if(Num != 0)
	{
		str_copy(pResult->m_aMessage, "Unable to load savegame: data corrupted");
		return true;
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
		return true;
	}

	int Since = pSqlServer->GetInt(2);
	if(Since < g_Config.m_SvSaveSwapGamesDelay)
	{
		str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage),
			"You have to wait %d seconds until you can load this savegame",
			g_Config.m_SvSaveSwapGamesDelay - Since);
		return true;
	}

	bool CanLoad = pResult->m_SavedTeam.MatchPlayers(
		pData->m_aClientNames, pData->m_aClientId, pData->m_NumPlayer,
		pResult->m_aMessage, sizeof(pResult->m_aMessage));

	if(!CanLoad)
		return true;

	char aSaveIdCondition[16] = "IS NULL";
	if(pResult->m_SaveId != UUID_NO_SAVE_ID)
	{
		str_format(aSaveIdCondition, sizeof(aSaveIdCondition), "= %s", pSqlServer->Placeholder(3).c_str());
	}
	str_format(aBuf, sizeof(aBuf),
		"DELETE FROM %s_save "
		"WHERE code = %s AND map_id = %s AND save_uuid %s",
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str(),
		aSaveIdCondition);
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindString(1, pData->m_aCode);
	pSqlServer->BindInt(2, MapId);
	char aUuid[UUID_MAXSTRSIZE];
	if(pResult->m_SaveId != UUID_NO_SAVE_ID)
	{
		FormatUuid(pResult->m_SaveId, aUuid, sizeof(aUuid));
		pSqlServer->BindString(3, aUuid);
	}
	pSqlServer->Print();
	int NumDeleted;
	if(!pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize))
	{
		return false;
	}

	if(NumDeleted != 1)
	{
		str_copy(pResult->m_aMessage, "Unable to load savegame: loaded on a different server");
		return true;
	}

	pResult->m_Status = CScoreSaveResult::LOAD_SUCCESS;
	str_copy(pResult->m_aMessage, "Loading successfully done");
	return true;
}

bool CScoreWorkerV2::GetSaves(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlPlayerRequest *>(pGameData);
	auto *pResult = dynamic_cast<CScorePlayerResult *>(pGameData->m_pResult.get());
	auto *paMessages = pResult->m_Data.m_aaMessages;

	int MapId;
	if(!GetMapId(pSqlServer, pData->m_aMap, &MapId, pError, ErrorSize))
	{
		return false;
	}

	char aSaveLike[128] = "";
	str_append(aSaveLike, "%\n");
	sqlstr::EscapeLike(aSaveLike + str_length(aSaveLike),
		pData->m_aRequestingPlayer,
		sizeof(aSaveLike) - str_length(aSaveLike));
	str_append(aSaveLike, "\t%");

	char aCurrentTimestamp[512];
	pSqlServer->ToUnixTimestamp("CURRENT_TIMESTAMP", aCurrentTimestamp, sizeof(aCurrentTimestamp));
	char aMaxTimestamp[512];
	pSqlServer->ToUnixTimestamp("MAX(saved_at)", aMaxTimestamp, sizeof(aMaxTimestamp));

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT COUNT(*) AS num_saves, %s-%s AS ago "
		"FROM %s_save "
		"WHERE map_id = %s AND savegame LIKE %s",
		aCurrentTimestamp, aMaxTimestamp,
		pSqlServer->GetPrefix(), pSqlServer->Placeholder(1).c_str(), pSqlServer->Placeholder(2).c_str());
	if(!pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	pSqlServer->BindInt(1, MapId);
	pSqlServer->BindString(2, aSaveLike);

	bool End;
	if(!pSqlServer->Step(&End, pError, ErrorSize))
	{
		return false;
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
	return true;
}
