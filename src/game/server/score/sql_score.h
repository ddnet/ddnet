/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include <exception>
#include <atomic>

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/console.h>
#include <engine/server/sql_server.h>
#include <engine/server/sql_connector.h>

#include "../score.h"

#include "sql_data.h"

class CGameContextError : public std::runtime_error
{
public:
	CGameContextError(const char* pMsg) : std::runtime_error(pMsg) {}
};

struct CGameDataBase
{
	CGameDataBase()
	{
		++ms_InstanceCount;
		m_GameInstance = ms_GameInstance;
	}
	virtual ~CGameDataBase() { --ms_InstanceCount; }

	bool isGameContextVaild() const
	{
		return m_GameInstance == ms_GameInstance && ms_GameContextAvailable;
	}

	CGameContext* GameServer() const { return isGameContextVaild() ? ms_pGameServer : throw CGameContextError("[CSqlData]: GameServer() unavailable."); }
	IServer* Server() const { return isGameContextVaild() ? ms_pServer : throw CGameContextError("[CSqlData]: Server() unavailable."); }
	CPlayerData* PlayerData(int ID) const { return isGameContextVaild() ? &ms_pPlayerData[ID] : throw CGameContextError("[CSqlData]: PlayerData() unavailable."); }

	// counter to keep track to which instance of GameServer this object belongs to.
	int m_GameInstance;

	static CGameContext *ms_pGameServer;
	static IServer *ms_pServer;
	static CPlayerData *ms_pPlayerData;

	static std::atomic_bool ms_GameContextAvailable;
	// contains the instancecount of the current GameServer
	static std::atomic_int ms_GameInstance;

	// keeps track of score-threads
	static std::atomic_int ms_InstanceCount;
};

template <typename T>
struct CGameData : public CGameDataBase
{
	CGameData(T* SqlData) : m_pSqlData(SqlData) {}
	virtual ~CGameData() { delete m_pSqlData; }

	const T& Sql() { return *m_pSqlData; }
private:
	T* m_pSqlData;
};

struct CTeamSaveGameData: public CGameData<CSqlTeamSave>
{
	CTeamSaveGameData(CSqlTeamSave* SqlData) : CGameData(SqlData) {}
	virtual ~CTeamSaveGameData();
};


template <typename T>
struct CSqlExecData
{
	CSqlExecData(bool (*pFuncPtr) (CSqlServer*, T&, bool), T* pData, bool ReadOnly = true) :
		m_pFuncPtr(pFuncPtr),
		m_pData(pData),
		m_ReadOnly(ReadOnly)
		{}

	bool (*m_pFuncPtr) (CSqlServer*, T&, bool);
	T* m_pData;
	bool m_ReadOnly;
};

template <typename T, typename U = CGameData<T>>
CSqlExecData<U>* newSqlExecData(bool (*pFuncPtr) (CSqlServer*, U&, bool), T* pData, bool ReadOnly = true)
{
	return new CSqlExecData<U>(pFuncPtr, new U(pData), ReadOnly);
}

class CSqlScore: public IScore
{
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	CGameContext *m_pGameServer;
	IServer *m_pServer;

	template <typename T>
	static void ExecSqlFunc(void *pUser)
	{
		CSqlExecData<T>* pData = (CSqlExecData<T> *)pUser;

		CSqlConnector connector;

		bool Success = false;

		// try to connect to a working databaseserver
		while (!Success && !connector.MaxTriesReached(pData->m_ReadOnly) && connector.ConnectSqlServer(pData->m_ReadOnly))
		{
			if (pData->m_pFuncPtr(connector.SqlServer(), *pData->m_pData, false))
				Success = true;

			// disconnect from databaseserver
			connector.SqlServer()->Disconnect();
		}

		// handle failures
		// eg write inserts to a file and print a nice error message
		if (!Success)
			pData->m_pFuncPtr(0, *pData->m_pData, true);

		delete pData->m_pData;
		delete pData;
	}

	static bool Init(CSqlServer* pSqlServer, CGameData<CSqlData>& pGameData, bool HandleFailure);

	static LOCK ms_FailureFileLock;

	static bool CheckBirthdayThread(CSqlServer* pSqlServer, CGameData<CSqlPlayerData>& pGameData, bool HandleFailure = false);
	static bool MapInfoThread(CSqlServer* pSqlServer, CGameData<CSqlMapData>& pGameData, bool HandleFailure = false);
	static bool MapVoteThread(CSqlServer* pSqlServer, CGameData<CSqlMapData>& pGameData, bool HandleFailure = false);
	static bool LoadScoreThread(CSqlServer* pSqlServer, CGameData<CSqlPlayerData>& pGameData, bool HandleFailure = false);
	static bool SaveScoreThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool SaveTeamScoreThread(CSqlServer* pSqlServer, CGameData<CSqlTeamScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowRankThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowTop5Thread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowTeamRankThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowTeamTop5Thread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowTimesThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowPointsThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool ShowTopPointsThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool RandomMapThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool RandomUnfinishedMapThread(CSqlServer* pSqlServer, CGameData<CSqlScoreData>& pGameData, bool HandleFailure = false);
	static bool SaveTeamThread(CSqlServer* pSqlServer, CTeamSaveGameData& pGameData, bool HandleFailure = false);
	static bool LoadTeamThread(CSqlServer* pSqlServer, CGameData<CSqlTeamLoad>& pGameData, bool HandleFailure = false);

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
