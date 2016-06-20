/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include <exception>
#include <string>

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/console.h>
#include <engine/external/json/json.h>
#include <engine/server/sql_connector.h>
#include <engine/server/sql_string_helpers.h>

#include "../score.h"

using json = nlohmann::json;


class CGameContextError : public std::runtime_error
{
public:
	CGameContextError(const char* pMsg) : std::runtime_error(pMsg) {}
};


// generic implementation to provide gameserver and server
struct CSqlData
{
	CSqlData() : m_Map(ms_pMap)
	{
		m_Instance = ms_Instance;
	}

	virtual ~CSqlData() {}

	bool isGameContextVaild() const
	{
		return m_Instance == ms_Instance && ms_GameContextAvailable;
	}

	CGameContext* GameServer() const { return isGameContextVaild() ? ms_pGameServer : throw CGameContextError("[CSqlData]: GameServer() unavailable."); }
	IServer* Server() const { return isGameContextVaild() ? ms_pServer : throw CGameContextError("[CSqlData]: Server() unavailable."); }
	CPlayerData* PlayerData(int ID) const { return isGameContextVaild() ? &ms_pPlayerData[ID] : throw CGameContextError("[CSqlData]: PlayerData() unavailable."); }

	sqlstr::CSqlString<128> m_Map;

	// counter to keep track to which instance of GameServer this object belongs to.
	int m_Instance;

	static CGameContext *ms_pGameServer;
	static IServer *ms_pServer;
	static CPlayerData *ms_pPlayerData;
	static const char *ms_pMap;

	static bool ms_GameContextAvailable;
	// contains the instancecount of the current GameServer
	static int ms_Instance;
};

struct CSqlExecData
{
	CSqlExecData(bool (*pFuncPtr) (CSqlServer*, const CSqlData *, bool), CSqlData *pSqlData, bool ReadOnly = true) :
		m_pFuncPtr(pFuncPtr),
		m_pSqlData(pSqlData),
		m_ReadOnly(ReadOnly)
	{
		++ms_InstanceCount;
	}
	~CSqlExecData()
	{
		--ms_InstanceCount;
	}

	bool (*m_pFuncPtr) (CSqlServer*, const CSqlData *, bool);
	CSqlData *m_pSqlData;
	bool m_ReadOnly;

	// keeps track of score-threads
	static int ms_InstanceCount;
};

struct CSqlPlayerData : CSqlData
{
	int m_ClientID;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;
};

// used for mapvote and mapinfo
struct CSqlMapData : CSqlData
{
	int m_ClientID;

	sqlstr::CSqlString<128> m_RequestedMap;
	char m_aFuzzyMap[128];
};

struct CSqlScoreData : CSqlData
{
	int m_ClientID;

	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer [MAX_NAME_LENGTH];
	char m_aTimestamp [20];
	sqlstr::CSqlString<sizeof(g_Config.m_SvSqlServerName)> m_ServerName;

	CSqlScoreData()
	{
		sqlstr::getTimeStamp(m_aTimestamp, sizeof(m_aTimestamp));
		m_ServerName = g_Config.m_SvSqlServerName;
	}

	json toJSON() const
	{
		json j = {
			{"type", "rank"},
			{"Map", m_Map.Str()},
			{"Name", m_Name.Str()},
			{"Timestamp", m_aTimestamp},
			{"Time", m_Time},
			{"Server", m_ServerName.Str()}
		};
		for (int i = 0; i < 25; i++)
		{
			char aBuf[5];
			str_format(aBuf, sizeof(aBuf), "cp%d", i);
			j[aBuf] = m_aCpCurrent[i];
		}
		return j;
	}

	void fromJSON(const char* pJStr)
	{
		try
		{
			json j = json::parse(pJStr);
			m_Map = j["Map"].get<std::string>().c_str();
			m_Name = j["Name"].get<std::string>().c_str();
			std::string timestamp = j["Timestamp"];
			str_copy(m_aTimestamp, timestamp.c_str(), timestamp.length());
			m_Time = j["Time"];
			m_ServerName = j["Server"].get<std::string>().c_str();
		}
		catch (const std::domain_error& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse rank json.");
			dbg_msg("sql", e.what());
		}
		catch (std::invalid_argument& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse rank json.");
			dbg_msg("sql", e.what());
		}
	}

};

struct CSqlTeamScoreData : CSqlData
{
	unsigned int m_Size;
	int m_aClientIDs[MAX_CLIENTS];
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_aNames [MAX_CLIENTS];
	float m_Time;
	char m_aTimestamp [20];
	sqlstr::CSqlString<sizeof(g_Config.m_SvSqlServerName)> m_ServerName;

	CSqlTeamScoreData()
	{
		sqlstr::getTimeStamp(m_aTimestamp, sizeof(m_aTimestamp));
		m_ServerName = g_Config.m_SvSqlServerName;
	}

	json toJSON() const
	{
		json j = {
			{"type", "teamrank"},
			{"Map", m_Map.Str()},
			{"Server", m_ServerName.Str()},
			{"Timestamp", m_aTimestamp},
			{"Time", m_Time}
		};

		json json_names = json::array();

		for (int i = 0; i < m_Size; i++)
		{
			json_names += m_aNames[i].Str();
		}
		j["Names"] = json_names;
		return j;
	}

	void fromJSON(const char* pJStr)
	{
		try
		{
			json j = json::parse(pJStr);
			m_Map = j["Map"].get<std::string>().c_str();
			std::string timestamp = j["Timestamp"];
			str_copy(m_aTimestamp, timestamp.c_str(), timestamp.length());
			m_Time = j["Time"];
			m_ServerName = j["Server"].get<std::string>().c_str();

			int i = 0;
			for (json& name : j["Names"])
			{
				if (i == MAX_CLIENTS)
					break;
				m_aNames[i++] = name.get<std::string>().c_str();
			}
		}
		catch (const std::domain_error& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse teamrank json.");
			dbg_msg("sql", e.what());
		}
		catch (std::invalid_argument& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse teamrank json.");
			dbg_msg("sql", e.what());
		}
	}
};

struct CSqlTeamSave : CSqlData
{
	virtual ~CSqlTeamSave();

	int m_Team;
	int m_ClientID;
	sqlstr::CSqlString<128> m_Code;
	char m_Server[5];
};

struct CSqlTeamLoad : CSqlData
{
	sqlstr::CSqlString<128> m_Code;
	int m_ClientID;
};

class CSqlScore: public IScore
{
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	CGameContext *m_pGameServer;
	IServer *m_pServer;

	static void ExecSqlFunc(void *pUser);

	static bool Init(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure);

	char m_aMap[64];

	static LOCK ms_FailureFileLock;

	static bool CheckBirthdayThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool MapInfoThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool MapVoteThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool LoadScoreThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool SaveScoreThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool SaveTeamScoreThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowRankThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTop5Thread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTeamRankThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTeamTop5Thread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTimesThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowPointsThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTopPointsThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool RandomMapThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool RandomUnfinishedMapThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool SaveTeamThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);
	static bool LoadTeamThread(CSqlServer* pSqlServer, const CSqlData *pGameData, bool HandleFailure = false);

public:

	CSqlScore(CGameContext *pGameServer);
	~CSqlScore();

	virtual void CheckBirthday(int ClientID);
	virtual void LoadScore(int ClientID);
	virtual void MapInfo(int ClientID, const char* MapName);
	virtual void MapVote(int ClientID, const char* MapName);
	virtual void SaveScore(int ClientID, float Time,
			float CpTime[NUM_CHECKPOINTS]);
	virtual void SaveTeamScore(int* aClientIDs, unsigned int Size, float Time);
	virtual void ShowRank(int ClientID, const char* pName, bool Search = false);
	virtual void ShowTeamRank(int ClientID, const char* pName, bool Search = false);
	virtual void ShowTimes(int ClientID, const char* pName, int Debut = 1);
	virtual void ShowTimes(int ClientID, int Debut = 1);
	virtual void ShowTop5(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void ShowTeamTop5(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void ShowPoints(int ClientID, const char* pName, bool Search = false);
	virtual void ShowTopPoints(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void RandomMap(int ClientID, int stars);
	virtual void RandomUnfinishedMap(int ClientID, int stars);
	virtual void SaveTeam(int Team, const char* Code, int ClientID, const char* Server);
	virtual void LoadTeam(const char* Code, int ClientID);

	virtual void OnShutdown();
};

#endif
