#ifndef GAME_SERVER_SCORE_H
#define GAME_SERVER_SCORE_H

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <engine/map.h>
#include <engine/server/databases/connection_pool.h>
#include <game/prng.h>
#include <game/voting.h>

#include "save.h"

struct ISqlData;
class IDbConnection;
class IServer;
class CGameContext;

enum
{
	NUM_CHECKPOINTS = 25,
	TIMESTAMP_STR_LENGTH = 20, // 2019-04-02 19:38:36
};

struct CScorePlayerResult : ISqlResult
{
	CScorePlayerResult();

	enum
	{
		MAX_MESSAGES = 10,
	};

	enum Variant
	{
		DIRECT,
		ALL,
		BROADCAST,
		MAP_VOTE,
		PLAYER_INFO,
	} m_MessageKind;
	union
	{
		char m_aaMessages[MAX_MESSAGES][512];
		char m_Broadcast[1024];
		struct
		{
			float m_Time;
			float m_CpTime[NUM_CHECKPOINTS];
			int m_Score;
			int m_HasFinishScore;
			int m_Birthday; // 0 indicates no birthday
		} m_Info;
		struct
		{
			char m_Reason[VOTE_REASON_LENGTH];
			char m_Server[32 + 1];
			char m_Map[MAX_MAP_LENGTH + 1];
		} m_MapVote;
	} m_Data; // PLAYER_INFO

	void SetVariant(Variant v);
};

struct CScoreRandomMapResult : ISqlResult
{
	CScoreRandomMapResult(int ClientID) :
		m_ClientID(ClientID)
	{
		m_Map[0] = '\0';
		m_aMessage[0] = '\0';
	}
	int m_ClientID;
	char m_Map[MAX_MAP_LENGTH];
	char m_aMessage[512];
};

struct CScoreSaveResult : ISqlResult
{
	CScoreSaveResult(int PlayerID, IGameController *Controller) :
		m_Status(SAVE_FAILED),
		m_SavedTeam(CSaveTeam(Controller)),
		m_RequestingPlayer(PlayerID)
	{
		m_aMessage[0] = '\0';
		m_aBroadcast[0] = '\0';
	}
	enum
	{
		SAVE_SUCCESS,
		// load team in the following two cases
		SAVE_FAILED,
		LOAD_SUCCESS,
		LOAD_FAILED,
	} m_Status;
	char m_aMessage[512];
	char m_aBroadcast[512];
	CSaveTeam m_SavedTeam;
	int m_RequestingPlayer;
	CUuid m_SaveID;
};

struct CScoreInitResult : ISqlResult
{
	CScoreInitResult() :
		m_CurrentRecord(0)
	{
	}
	float m_CurrentRecord;
};

class CPlayerData
{
public:
	CPlayerData()
	{
		Reset();
	}
	~CPlayerData() {}

	void Reset()
	{
		m_BestTime = 0;
		m_CurrentTime = 0;
		for(float &BestCpTime : m_aBestCpTime)
			BestCpTime = 0;
	}

	void Set(float Time, float CpTime[NUM_CHECKPOINTS])
	{
		m_BestTime = Time;
		m_CurrentTime = Time;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestCpTime[i] = CpTime[i];
	}

	float m_BestTime;
	float m_CurrentTime;
	float m_aBestCpTime[NUM_CHECKPOINTS];
};

struct CSqlInitData : ISqlData
{
	CSqlInitData(std::shared_ptr<CScoreInitResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	// current map
	char m_Map[MAX_MAP_LENGTH];
};

struct CSqlPlayerRequest : ISqlData
{
	CSqlPlayerRequest(std::shared_ptr<CScorePlayerResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	// object being requested, either map (128 bytes) or player (16 bytes)
	char m_Name[MAX_MAP_LENGTH];
	// current map
	char m_Map[MAX_MAP_LENGTH];
	char m_RequestingPlayer[MAX_NAME_LENGTH];
	// relevant for /top5 kind of requests
	int m_Offset;
	char m_Server[5];
};

struct CSqlRandomMapRequest : ISqlData
{
	CSqlRandomMapRequest(std::shared_ptr<CScoreRandomMapResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_ServerType[32];
	char m_CurrentMap[MAX_MAP_LENGTH];
	char m_RequestingPlayer[MAX_NAME_LENGTH];
	int m_Stars;
};

struct CSqlScoreData : ISqlData
{
	CSqlScoreData(std::shared_ptr<CScorePlayerResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	virtual ~CSqlScoreData(){};

	char m_Map[MAX_MAP_LENGTH];
	char m_GameUuid[UUID_MAXSTRSIZE];
	char m_Name[MAX_MAP_LENGTH];

	int m_ClientID;
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CSqlTeamScoreData : ISqlData
{
	CSqlTeamScoreData() :
		ISqlData(nullptr)
	{
	}

	char m_GameUuid[UUID_MAXSTRSIZE];
	char m_Map[MAX_MAP_LENGTH];
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	unsigned int m_Size;
	char m_aNames[MAX_CLIENTS][MAX_NAME_LENGTH];
};

struct CSqlTeamSave : ISqlData
{
	CSqlTeamSave(std::shared_ptr<CScoreSaveResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}
	virtual ~CSqlTeamSave(){};

	char m_ClientName[MAX_NAME_LENGTH];
	char m_Map[MAX_MAP_LENGTH];
	char m_Code[128];
	char m_aGeneratedCode[128];
	char m_Server[5];
};

struct CSqlTeamLoad : ISqlData
{
	CSqlTeamLoad(std::shared_ptr<CScoreSaveResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}
	virtual ~CSqlTeamLoad(){};

	char m_Code[128];
	char m_Map[MAX_MAP_LENGTH];
	char m_RequestingPlayer[MAX_NAME_LENGTH];
	int m_ClientID;
	// struct holding all player names in the team or an empty string
	char m_aClientNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	int m_aClientID[MAX_CLIENTS];
	int m_NumPlayer;
};

struct CTeamrank
{
	CUuid m_TeamID;
	char m_aaNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	unsigned int m_NumNames;
	CTeamrank();

	// Assumes that a database query equivalent to
	//
	//     SELECT TeamID, Name [, ...] -- the order is important
	//     FROM record_teamrace
	//     ORDER BY TeamID, Name
	//
	// was executed and that the result line of the first team member is already selected.
	// Afterwards the team member of the next team is selected.
	//
	// Returns true on SQL failure
	//
	// if another team can be extracted
	bool NextSqlResult(IDbConnection *pSqlServer, bool *pEnd, char *pError, int ErrorSize);

	bool SamePlayers(const std::vector<std::string> *aSortedNames);
};

class CScore
{
	CPlayerData m_aPlayerData[MAX_CLIENTS];
	CDbConnectionPool *m_pPool;

	static bool Init(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool RandomMapThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool RandomUnfinishedMapThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool MapVoteThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool LoadPlayerDataThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool MapInfoThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowRankThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTeamRankThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTopThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTeamTop5Thread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowPlayerTeamTop5Thread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTimesThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowPointsThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTopPointsThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool GetSavesThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool SaveTeamThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);
	static bool LoadTeamThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);

	static bool SaveScoreThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);
	static bool SaveTeamScoreThread(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }
	CGameContext *m_pGameServer;
	IServer *m_pServer;

	std::vector<std::string> m_aWordlist;
	CPrng m_Prng;
	void GeneratePassphrase(char *pBuf, int BufSize);

	// returns new SqlResult bound to the player, if no current Thread is active for this player
	std::shared_ptr<CScorePlayerResult> NewSqlPlayerResult(int ClientID);
	// Creates for player database requests
	void ExecPlayerThread(
		bool (*pFuncPtr)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize),
		const char *pThreadName,
		int ClientID,
		const char *pName,
		int Offset);

	// returns true if the player should be rate limited
	bool RateLimitPlayer(int ClientID);

public:
	CScore(CGameContext *pGameServer, CDbConnectionPool *pPool);
	~CScore() {}

	CPlayerData *PlayerData(int ID) { return &m_aPlayerData[ID]; }

	void MapInfo(int ClientID, const char *pMapName);
	void MapVote(int ClientID, const char *pMapName);
	void LoadPlayerData(int ClientID);
	void SaveScore(int ClientID, float Time, const char *pTimestamp, float aCpTime[NUM_CHECKPOINTS], bool NotEligible);

	void SaveTeamScore(int *pClientIDs, unsigned int Size, float Time, const char *pTimestamp);

	void ShowTop(int ClientID, int Offset = 1);
	void ShowRank(int ClientID, const char *pName);

	void ShowTeamTop5(int ClientID, int Offset = 1);
	void ShowTeamTop5(int ClientID, const char *pName, int Offset = 1);
	void ShowTeamRank(int ClientID, const char *pName);

	void ShowTopPoints(int ClientID, int Offset = 1);
	void ShowPoints(int ClientID, const char *pName);

	void ShowTimes(int ClientID, const char *pName, int Offset = 1);
	void ShowTimes(int ClientID, int Offset = 1);

	void RandomMap(int ClientID, int Stars);
	void RandomUnfinishedMap(int ClientID, int Stars);

	void SaveTeam(int ClientID, const char *pCode, const char *pServer);
	void LoadTeam(const char *pCode, int ClientID);
	void GetSaves(int ClientID);
};

#endif // GAME_SERVER_SCORE_H
