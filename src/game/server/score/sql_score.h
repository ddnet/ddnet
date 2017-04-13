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
#include <engine/shared/sql_server.h>
#include <engine/shared/sql_connector.h>

#include "../score.h"

#include "sql_data.h"

/*
CGameContextError:

	Error to be thrown if an attempt is made to access gamespecific data while those are
	unavailable.
*/
class CGameContextError : public std::runtime_error
{
public:
	CGameContextError(const char* pMsg) : std::runtime_error(pMsg) {}
};


/*
CGameDataBase:

	This struct provides access to gamespecific things and takes care that they actually can be
	accessed.
	Additionally it keeps track of the running threads that perform sql queries and can is used
	to sync them with the main thread on shutdown. This is done with static variables. In order
	to keep those variables globally unique all more specific classes for CGameDataBase inherit
	from this class.
*/
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


/*
CGameData:

	This struct provides access to the actual data that are required by an sql-task. It inherits
	from CGameDataBase and does not define the logic from it itself as this struct is a template and
	thus could create multiple instances of members declared as static here.

	As templateargument the type of the data specific to the sql-taks is required, see sql_data.h.
*/
template <typename T>
struct CGameData : public CGameDataBase
{
	CGameData(T* SqlData) : m_pSqlData(SqlData) {}
	virtual ~CGameData() { delete m_pSqlData; }

	// Access to task-specific data.
	const T& Data() { return *m_pSqlData; }
private:
	T* m_pSqlData;
};

/*
CTeamSaveGameData:

	Provide a custom destructor for cleanup.
*/
struct CTeamSaveGameData: public CGameData<CSqlTeamSave>
{
	CTeamSaveGameData(CSqlTeamSave* SqlData) : CGameData(SqlData) {}
	virtual ~CTeamSaveGameData();
};


/*
CSqlExecData:

	This struct defines the data that will be supplied for ExecSqlFunc mostly by a threadinvokation.

	It contains the function that will perform the sql-queries as well as the data to supply to it
	and whether to use a readonly sql-server.
*/
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

/*
	This function creates a new instance CSqlExecData on the heap. It will supply it with an
	instance of CGameData if not specified differently, also created on the heap. Therefor it will
	fill the new instance of CGameData with pData.

	This simplifies creating new CSqlExecData as it will mostly have some specialization of
	CGameData as Data. Like this it is enough to invoke this function like this to aquire a brandnew
	instance of CSqlExecData:

	CSqlExecData* pExecData = newSqlExecData(pSomeSqlFunction, pSomeTaskSpecificData);

	Templatededcution will get the appropriate types from pSomeTaskSpecificData and there is no need
	to worry about those: <>

	ExecSqlFunc will delete those objects when it is done.
*/
template <typename T, typename U = CGameData<T>>
CSqlExecData<U>* newSqlExecData(bool (*pFuncPtr) (CSqlServer*, U&, bool), T* pData, bool ReadOnly = true)
{
	return new CSqlExecData<U>(pFuncPtr, new U(pData), ReadOnly);
}

/*
 The idea:

 	This function takes everything that is required for a database action as CSqlExecData and tries
	to perfrom the given task until it is reported as done. To achieve this it will try all
	specified sql-fallbacks one by one on an error until the task is done or no fallbacks are left.

	As the sql-stuff is threaded this function only accepts a void* which will be casted to
	CSqlExecData*. For greater typesafety it is a template and expects the type of data to supply
	to the function that performs the sql stuff.

Performing a task looks like this:

	The function that is stored in CSqlExecData will be invoked with the arguments:
	(sql-server to use, supplied data from CSqlExecData, whether to handle failure)

	It is expected to return if the task is done or if it should be invoked again with a fallback-
	server. If there are no more fallbacks left HandleFailure will be set to true and action can be
	taken.
*/
template <typename T>
static void ExecSqlFunc(void *pUser)
{
	CSqlExecData<T>* pData = (CSqlExecData<T> *)pUser;

	CSqlConnector connector;

	bool Done = false;

	// try to connect to a working databaseserver
	while (!Done && !connector.MaxTriesReached(pData->m_ReadOnly) && connector.ConnectSqlServer(pData->m_ReadOnly))
	{
		if (pData->m_pFuncPtr(connector.SqlServer(), *pData->m_pData, false))
			Done = true;

		// disconnect from databaseserver
		connector.SqlServer()->Disconnect();
	}

	// handle failures
	// eg write inserts to a file and print a nice error message
	if (!Done)
		pData->m_pFuncPtr(0, *pData->m_pData, true);

	delete pData->m_pData;
	delete pData;
}

class CSqlScore: public IScore
{
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	CGameContext *m_pGameServer;
	IServer *m_pServer;

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
