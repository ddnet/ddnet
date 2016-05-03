/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include <exception>

#include <engine/console.h>
#include <engine/server/sql_connector.h>

#include "../score.h"


class CGameContextError : public std::runtime_error
{
public:
	CGameContextError(const char* pMsg) : std::runtime_error(pMsg) {}
};


// generic implementation to provide gameserver and server
struct CSqlData
{
	CSqlData()
	{
		m_Instance = ms_Instance;
		str_copy(m_aMap, ms_pMap, sizeof(m_aMap));
	}

	virtual ~CSqlData() {}

	bool isGameContextVaild()
	{
		return m_Instance == ms_Instance && ms_GameContextAvailable;
	}

	CGameContext* GameServer() { return isGameContextVaild() ? ms_pGameServer : throw CGameContextError("[CSqlData]: GameServer() unavailable."); }
	IServer* Server() { return isGameContextVaild() ? ms_pServer : throw CGameContextError("[CSqlData]: Server() unavailable."); }
	CPlayerData* PlayerData(int ID) { return isGameContextVaild() ? &ms_pPlayerData[ID] : throw CGameContextError("[CSqlData]: PlayerData() unavailable."); }
	const char* MapName() { return m_aMap; }

	char m_aMap[128];

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
	CSqlExecData(bool (*pFuncPtr) (CSqlServer*, CSqlData *, bool), CSqlData *pSqlData, bool ReadOnly = true) :
		m_pFuncPtr(pFuncPtr),
		m_pSqlData(pSqlData),
		m_ReadOnly(ReadOnly)
	{}
	bool (*m_pFuncPtr) (CSqlServer*, CSqlData *, bool);
	CSqlData *m_pSqlData;
	bool m_ReadOnly;
};

// used for mapvote and mapinfo
struct CSqlMapData : CSqlData
{
	int m_ClientID;
	char m_aMap[128];
};

struct CSqlScoreData : CSqlData
{
	int m_ClientID;
#if defined(CONF_FAMILY_WINDOWS)
	char m_aName[16]; // Don't edit this, or all your teeth will fall http://bugs.mysql.com/bug.php?id=50046
#else
	char m_aName[MAX_NAME_LENGTH * 2 - 1];
#endif

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CSqlTeamScoreData : CSqlData
{
	unsigned int m_Size;
	int m_aClientIDs[MAX_CLIENTS];
#if defined(CONF_FAMILY_WINDOWS)
	char m_aNames[16][MAX_CLIENTS]; // Don't edit this, or all your teeth will fall http://bugs.mysql.com/bug.php?id=50046
#else
	char m_aNames[MAX_NAME_LENGTH * 2 - 1][MAX_CLIENTS];
#endif

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CSqlTeamSave : CSqlData
{
	int m_Team;
	int m_ClientID;
	char m_Code[128];
	char m_Server[5];
};

struct CSqlTeamLoad : CSqlData
{
	char m_Code[128];
	int m_ClientID;
};

class CSqlScore: public IScore
{
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	CGameContext *m_pGameServer;
	IServer *m_pServer;

	static void ExecSqlFunc(void *pUser);

	static bool Init(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure);

	char m_aMap[64];

	static bool MapInfoThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool MapVoteThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool LoadScoreThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool SaveScoreThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool SaveTeamScoreThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowRankThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTop5Thread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTeamRankThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTeamTop5Thread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTimesThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowPointsThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool ShowTopPointsThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool RandomMapThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool RandomUnfinishedMapThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool SaveTeamThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);
	static bool LoadTeamThread(CSqlServer* pSqlServer, CSqlData *pGameData, bool HandleFailure = false);

public:

	CSqlScore(CGameContext *pGameServer);
	~CSqlScore();

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
};

#endif
