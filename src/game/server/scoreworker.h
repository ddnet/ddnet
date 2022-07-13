#ifndef GAME_SERVER_SCOREWORKER_H
#define GAME_SERVER_SCOREWORKER_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <engine/map.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/shared/protocol.h>
#include <engine/shared/uuid_manager.h>
#include <game/server/save.h>
#include <game/voting.h>

class IDbConnection;
class IGameController;

enum
{
	NUM_CHECKPOINTS = MAX_CHECKPOINTS,
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
	union UPlayerInfo
	{
		char m_aaMessages[MAX_MESSAGES][512] = {{0}};
		char m_aBroadcast[1024] /* = {0} */;
		struct
		{
			float m_Time = 0;
			float m_aTimeCp[NUM_CHECKPOINTS] = {0};
			int m_Score = 0;
			int m_HasFinishScore = 0;
			int m_Birthday = 0; // 0 indicates no birthday
		} m_Info;
		struct
		{
			char m_aReason[VOTE_REASON_LENGTH] = {0};
			char m_aServer[32 + 1] = {0};
			char m_aMap[MAX_MAP_LENGTH + 1] = {0};
		} m_MapVote;
		UPlayerInfo()
		{
			mem_zero(this, sizeof(*this));
		}
	} m_Data; // PLAYER_INFO

	void SetVariant(Variant v);
};

struct CScoreInitResult : ISqlResult
{
	CScoreInitResult() :
		m_CurrentRecord(0)
	{
	}
	float m_CurrentRecord = 0;
};

struct CSqlInitData : ISqlData
{
	CSqlInitData(std::shared_ptr<CScoreInitResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	// current map
	char m_aMap[MAX_MAP_LENGTH] = {0};
};

struct CSqlPlayerRequest : ISqlData
{
	CSqlPlayerRequest(std::shared_ptr<CScorePlayerResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	// object being requested, either map (128 bytes) or player (16 bytes)
	char m_aName[MAX_MAP_LENGTH] = {0};
	// current map
	char m_aMap[MAX_MAP_LENGTH] = {0};
	char m_aRequestingPlayer[MAX_NAME_LENGTH] = {0};
	// relevant for /top5 kind of requests
	int m_Offset = 0;
	char m_aServer[5] = {0};
};

struct CScoreRandomMapResult : ISqlResult
{
	CScoreRandomMapResult(int ClientID) :
		m_ClientID(ClientID)
	{
		m_aMap[0] = '\0';
		m_aMessage[0] = '\0';
	}
	int m_ClientID = 0;
	char m_aMap[MAX_MAP_LENGTH] = {0};
	char m_aMessage[512] = {0};
};

struct CSqlRandomMapRequest : ISqlData
{
	CSqlRandomMapRequest(std::shared_ptr<CScoreRandomMapResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aServerType[32] = {0};
	char m_aCurrentMap[MAX_MAP_LENGTH] = {0};
	char m_aRequestingPlayer[MAX_NAME_LENGTH] = {0};
	int m_Stars = 0;
};

struct CSqlScoreData : ISqlData
{
	CSqlScoreData(std::shared_ptr<CScorePlayerResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	virtual ~CSqlScoreData(){};

	char m_aMap[MAX_MAP_LENGTH] = {0};
	char m_aGameUuid[UUID_MAXSTRSIZE] = {0};
	char m_aName[MAX_MAP_LENGTH] = {0};

	int m_ClientID = 0;
	float m_Time = 0;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH] = {0};
	float m_aCurrentTimeCp[NUM_CHECKPOINTS] = {0};
	int m_Num = 0;
	bool m_Search = false;
	char m_aRequestingPlayer[MAX_NAME_LENGTH] = {0};
};

struct CScoreSaveResult : ISqlResult
{
	CScoreSaveResult(int PlayerID, IGameController *pController) :
		m_Status(SAVE_FAILED),
		m_SavedTeam(CSaveTeam(pController)),
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
	char m_aMessage[512] = {0};
	char m_aBroadcast[512] = {0};
	CSaveTeam m_SavedTeam;
	int m_RequestingPlayer = 0;
	CUuid m_SaveID;
};

struct CSqlTeamScoreData : ISqlData
{
	CSqlTeamScoreData() :
		ISqlData(nullptr)
	{
	}

	char m_aGameUuid[UUID_MAXSTRSIZE] = {0};
	char m_aMap[MAX_MAP_LENGTH] = {0};
	float m_Time = 0;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH] = {0};
	unsigned int m_Size = 0;
	char m_aaNames[MAX_CLIENTS][MAX_NAME_LENGTH] = {{0}};
};

struct CSqlTeamSave : ISqlData
{
	CSqlTeamSave(std::shared_ptr<CScoreSaveResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}
	virtual ~CSqlTeamSave(){};

	char m_aClientName[MAX_NAME_LENGTH] = {0};
	char m_aMap[MAX_MAP_LENGTH] = {0};
	char m_aCode[128] = {0};
	char m_aGeneratedCode[128] = {0};
	char m_aServer[5] = {0};
};

struct CSqlTeamLoad : ISqlData
{
	CSqlTeamLoad(std::shared_ptr<CScoreSaveResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}
	virtual ~CSqlTeamLoad(){};

	char m_aCode[128] = {0};
	char m_aMap[MAX_MAP_LENGTH] = {0};
	char m_aRequestingPlayer[MAX_NAME_LENGTH] = {0};
	int m_ClientID = 0;
	// struct holding all player names in the team or an empty string
	char m_aaClientNames[MAX_CLIENTS][MAX_NAME_LENGTH] = {{0}};
	int m_aClientID[MAX_CLIENTS] = {0};
	int m_NumPlayer = 0;
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
		for(float &BestTimeCp : m_aBestTimeCp)
			BestTimeCp = 0;
	}

	void Set(float Time, float aTimeCp[NUM_CHECKPOINTS])
	{
		m_BestTime = Time;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestTimeCp[i] = aTimeCp[i];
	}

	float m_BestTime = 0;
	float m_aBestTimeCp[NUM_CHECKPOINTS] = {0};
};

struct CTeamrank
{
	CUuid m_TeamID;
	char m_aaNames[MAX_CLIENTS][MAX_NAME_LENGTH] = {{0}};
	unsigned int m_NumNames = 0;
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

	bool SamePlayers(const std::vector<std::string> *pvSortedNames);
};

struct CScoreWorker
{
	static bool Init(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool RandomMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool RandomUnfinishedMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool MapVote(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool LoadPlayerData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool MapInfo(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowRank(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTeamRank(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTop(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTeamTop5(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowPlayerTeamTop5(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTimes(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowPoints(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ShowTopPoints(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool GetSaves(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool SaveTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);
	static bool LoadTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);

	static bool SaveScore(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);
	static bool SaveTeamScore(IDbConnection *pSqlServer, const ISqlData *pGameData, bool Failure, char *pError, int ErrorSize);
};

#endif // GAME_SERVER_SCOREWORKER_H
