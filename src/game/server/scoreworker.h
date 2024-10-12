#ifndef GAME_SERVER_SCOREWORKER_H
#define GAME_SERVER_SCOREWORKER_H

#include <memory>
#include <optional>
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
		PLAYER_TIMECP,
	} m_MessageKind;
	union
	{
		char m_aaMessages[MAX_MESSAGES][512];
		char m_aBroadcast[1024];
		struct
		{
			std::optional<float> m_Time;
			float m_aTimeCp[NUM_CHECKPOINTS];
			int m_Birthday; // 0 indicates no birthday
			char m_aRequestedPlayer[MAX_NAME_LENGTH];
		} m_Info = {};
		struct
		{
			char m_aReason[VOTE_REASON_LENGTH];
			char m_aServer[32 + 1];
			char m_aMap[MAX_MAP_LENGTH + 1];
		} m_MapVote;
	} m_Data = {}; // PLAYER_INFO

	void SetVariant(Variant v);
};

struct CScoreLoadBestTimeResult : ISqlResult
{
	CScoreLoadBestTimeResult() :
		m_CurrentRecord(0)
	{
	}
	float m_CurrentRecord;
};

struct CSqlLoadBestTimeRequest : ISqlData
{
	CSqlLoadBestTimeRequest(std::shared_ptr<CScoreLoadBestTimeResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	// current map
	char m_aMap[MAX_MAP_LENGTH];
};

struct CSqlPlayerRequest : ISqlData
{
	CSqlPlayerRequest(std::shared_ptr<CScorePlayerResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	// object being requested, either map (128 bytes) or player (16 bytes)
	char m_aName[MAX_MAP_LENGTH];
	// current map
	char m_aMap[MAX_MAP_LENGTH];
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
	// relevant for /top5 kind of requests
	int m_Offset;
	char m_aServer[5];
};

struct CScoreRandomMapResult : ISqlResult
{
	CScoreRandomMapResult(int ClientId) :
		m_ClientId(ClientId)
	{
		m_aMap[0] = '\0';
		m_aMessage[0] = '\0';
	}
	int m_ClientId;
	char m_aMap[MAX_MAP_LENGTH];
	char m_aMessage[512];
};

struct CSqlRandomMapRequest : ISqlData
{
	CSqlRandomMapRequest(std::shared_ptr<CScoreRandomMapResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aServerType[32];
	char m_aCurrentMap[MAX_MAP_LENGTH];
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
	int m_Stars;
};

struct CSqlScoreData : ISqlData
{
	CSqlScoreData(std::shared_ptr<CScorePlayerResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	virtual ~CSqlScoreData(){};

	char m_aMap[MAX_MAP_LENGTH];
	char m_aGameUuid[UUID_MAXSTRSIZE];
	char m_aName[MAX_MAP_LENGTH];

	int m_ClientId;
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	float m_aCurrentTimeCp[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CScoreSaveResult : ISqlResult
{
	CScoreSaveResult(int PlayerId) :
		m_Status(SAVE_FAILED),
		m_RequestingPlayer(PlayerId)
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
	CUuid m_SaveId;
};

struct CSqlTeamScoreData : ISqlData
{
	CSqlTeamScoreData() :
		ISqlData(nullptr)
	{
	}

	char m_aGameUuid[UUID_MAXSTRSIZE];
	char m_aMap[MAX_MAP_LENGTH];
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	unsigned int m_Size;
	char m_aaNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	CUuid m_TeamrankUuid;
};

struct CSqlTeamSaveData : ISqlData
{
	CSqlTeamSaveData(std::shared_ptr<CScoreSaveResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}
	virtual ~CSqlTeamSaveData(){};

	char m_aClientName[MAX_NAME_LENGTH];
	char m_aMap[MAX_MAP_LENGTH];
	char m_aCode[128];
	char m_aGeneratedCode[128];
	char m_aServer[5];
};

struct CSqlTeamLoadRequest : ISqlData
{
	CSqlTeamLoadRequest(std::shared_ptr<CScoreSaveResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}
	virtual ~CSqlTeamLoadRequest(){};

	char m_aCode[128];
	char m_aMap[MAX_MAP_LENGTH];
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
	// struct holding all player names in the team or an empty string
	char m_aClientNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	int m_aClientId[MAX_CLIENTS];
	int m_NumPlayer;
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

		m_RecordStopTick = -1;
	}

	void Set(float Time, const float aTimeCp[NUM_CHECKPOINTS])
	{
		m_BestTime = Time;
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestTimeCp[i] = aTimeCp[i];
	}

	void SetBestTimeCp(const float aTimeCp[NUM_CHECKPOINTS])
	{
		for(int i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestTimeCp[i] = aTimeCp[i];
	}

	float m_BestTime;
	float m_aBestTimeCp[NUM_CHECKPOINTS];

	int m_RecordStopTick;
	float m_RecordFinishTime;
};

struct CTeamrank
{
	CUuid m_TeamId;
	char m_aaNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	unsigned int m_NumNames;
	CTeamrank();

	// Assumes that a database query equivalent to
	//
	//     SELECT TeamId, Name [, ...] -- the order is important
	//     FROM record_teamrace
	//     ORDER BY TeamId, Name
	//
	// was executed and that the result line of the first team member is already selected.
	// Afterwards the team member of the next team is selected.
	//
	// Returns true on SQL failure
	//
	// if another team can be extracted
	bool NextSqlResult(IDbConnection *pSqlServer, bool *pEnd, char *pError, int ErrorSize);

	bool SamePlayers(const std::vector<std::string> *pvSortedNames);

	static bool GetSqlTop5Team(IDbConnection *pSqlServer, bool *pEnd, char *pError, int ErrorSize, char (*paMessages)[512], int *StartLine, int Count);
};

struct CScoreWorker
{
	static bool LoadBestTime(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool RandomMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool RandomUnfinishedMap(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool MapVote(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	static bool LoadPlayerData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool LoadPlayerTimeCp(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
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

	static bool SaveTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);
	static bool LoadTeam(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);

	static bool SaveScore(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);
	static bool SaveTeamScore(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);
};

#endif // GAME_SERVER_SCOREWORKER_H
