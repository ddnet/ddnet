/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SCORE_SQL_H
#define GAME_SERVER_SCORE_SQL_H

#include <engine/server/sql_string_helpers.h>

#include "../score.h"

class CSqlServer;

class CSqlPlayerResult {
public:
	std::atomic_bool m_Done;
	CSqlPlayerResult();

	enum Variant
	{
		DIRECT,
		ALL,
		MAP_VOTE, // 3 Messages: Reason, Server, Map
		PLAYER_INFO,
	} m_MessageKind;
	char m_aaMessages[7][512];
	union {
		struct {
			float m_Time;
			float m_CpTime[NUM_CHECKPOINTS];
			int m_Score;
			int m_HasFinishScore;
			int m_Birthday; // 0 indicates no birthday
		} m_Info;
	} m_Data; // PLAYER_INFO

	void SetVariant(Variant v);
};

class CSqlSaveResult {
public:
	CSqlSaveResult(int PlayerID, IGameController* Controller) :
		m_Status(SAVE_SUCCESS),
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
};

class CSqlMapResult {
	CSqlMapResult();
	std::atomic_bool m_Done;
};

// result only valid if m_Done is set to true
class CSqlResult
{
public:
	std::atomic_bool m_Done;
	// specify where chat messages should be returned
	enum
	{
		DIRECT,
		TEAM,
		ALL,
	} m_MessageTarget;
	int m_TeamMessageTo; // store team id, if player changes team after /save
	char m_Message[512];
	// TODO: replace this with a type-safe std::variant (C++17)
	enum
	{
		NONE,
		LOAD,
		RANDOM_MAP,
		MAP_VOTE,
	} m_Tag;

	union
	{
		//CSaveTeam m_LoadTeam;
		CRandomMapResult m_RandomMap;
		CMapVoteResult m_MapVote;
	} m_Variant;

	CSqlResult();
	~CSqlResult();
};

// holding relevant data for one thread, and function pointer for return values
template < typename TResult >
struct CSqlData
{
	CSqlData(std::shared_ptr<TResult> pSqlResult) :
		m_pResult(pSqlResult)
	{ }
	std::shared_ptr<TResult> m_pResult;
	virtual ~CSqlData() = default;
};

// used for mapinfo
struct CSqlMapData : CSqlData<CSqlPlayerResult>
{
	using CSqlData<CSqlPlayerResult>::CSqlData;
	int m_ClientID;

	sqlstr::CSqlString<128> m_RequestedMap;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;
};

struct CSqlPlayerRequest : CSqlData<CSqlPlayerResult>
{
	using CSqlData<CSqlPlayerResult>::CSqlData;
	// object being requested, either map (128 bytes) or player (16 bytes)
	sqlstr::CSqlString<128> m_Name;
	// current map
	sqlstr::CSqlString<128> m_Map;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_RequestingPlayer;
	// relevant for /top5 kind of requests
	int m_Offset;
};

// used for mapvote
struct CSqlMapVoteData : CSqlMapData
{
	std::shared_ptr<CMapVoteResult> m_pResult;
};

struct CSqlScoreData : CSqlData<CSqlResult>
{
	int m_ClientID;

	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;

	bool m_NotEligible;
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CSqlTeamScoreData : CSqlData<CSqlResult>
{
	bool m_NotEligible;
	float m_Time;
	char m_aTimestamp[TIMESTAMP_STR_LENGTH];
	unsigned int m_Size;
	int m_aClientIDs[MAX_CLIENTS];
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_aNames[MAX_CLIENTS];
};

struct CSqlTeamSave : CSqlData<CSqlSaveResult>
{
	using CSqlData<CSqlSaveResult>::CSqlData;
	virtual ~CSqlTeamSave() {};

	char m_ClientName[MAX_NAME_LENGTH];

	sqlstr::CSqlString<128> m_Map;
	sqlstr::CSqlString<128> m_Code;
	char m_Uuid[UUID_MAXSTRSIZE];
	char m_Server[5];
};

struct CSqlTeamLoad : CSqlData<CSqlSaveResult>
{
	using CSqlData<CSqlSaveResult>::CSqlData;
	sqlstr::CSqlString<128> m_Code;
	sqlstr::CSqlString<128> m_Map;
	sqlstr::CSqlString<128> m_RequestingPlayer;
	int m_ClientID;
	// struct holding all player names in the team or an empty string
	char m_aClientNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	int m_aClientID[MAX_CLIENTS];
	int m_NumPlayer;
};

struct CSqlRandomMap : CSqlData<CSqlResult>
{
	using CSqlData::CSqlData;
	int m_Stars;
	char m_aCurrentMap[64];
	char m_aServerType[64];
};

// controls one thread
template < typename TResult >
struct CSqlExecData
{
	CSqlExecData(
			bool (*pFuncPtr) (CSqlServer*, const CSqlData<TResult> *, bool),
			CSqlData<TResult> *pSqlResult,
			bool ReadOnly = true
	);
	~CSqlExecData();

	bool (*m_pFuncPtr) (CSqlServer*, const CSqlData<TResult>  *, bool);
	CSqlData<TResult> *m_pSqlData;
	bool m_ReadOnly;

	static void ExecSqlFunc(void *pUser);
};

class IServer;
class CGameContext;

class CSqlScore: public IScore
{
	static LOCK ms_FailureFileLock;


	static bool Init(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure);

	static bool RandomMapThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure = false);
	static bool RandomUnfinishedMapThread(CSqlServer* pSqlServer, const CSqlData<CSqlResult> *pGameData, bool HandleFailure = false);
	static bool MapVoteThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);

	static bool LoadPlayerDataThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool MapInfoThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowRankThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTeamRankThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTop5Thread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTeamTop5Thread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTimesThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowPointsThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool ShowTopPointsThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool GetSavesThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);

	static bool SaveTeamThread(CSqlServer* pSqlServer, const CSqlData<CSqlSaveResult> *pGameData, bool HandleFailure = false);
	static bool LoadTeamThread(CSqlServer* pSqlServer, const CSqlData<CSqlSaveResult> *pGameData, bool HandleFailure = false);

	static bool SaveScoreThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);
	static bool SaveTeamScoreThread(CSqlServer* pSqlServer, const CSqlData<CSqlPlayerResult> *pGameData, bool HandleFailure = false);

	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	CGameContext *m_pGameServer;
	IServer *m_pServer;

	char m_aMap[64];
	char m_aGameUuid[UUID_MAXSTRSIZE];

	// returns new SqlResult bound to the player, if no current Thread is active for this player
	std::shared_ptr<CSqlPlayerResult> NewSqlPlayerResult(int ClientID);
	// Creates for player database requests
	void ExecPlayerThread(
			bool (*pFuncPtr) (CSqlServer*, const CSqlData<CSqlPlayerResult> *, bool),
			const char* pThreadName,
			int ClientID,
			const char* pName,
			int Offset
	);

public:
	// keeps track of score-threads
	static std::atomic_int ms_InstanceCount;

	CSqlScore(CGameContext *pGameServer);
	~CSqlScore() {}

	// Requested by game context, shouldn't fail in case the player started another thread
	virtual void RandomMap(int ClientID, int Stars);
	virtual void RandomUnfinishedMap(int ClientID, int Stars);
	virtual void MapVote(int ClientID, const char* MapName);

	virtual void LoadPlayerData(int ClientID);
	// Requested by players (fails if another request by this player is active)
	virtual void MapInfo(int ClientID, const char* MapName);
	virtual void ShowRank(int ClientID, const char* pName);
	virtual void ShowTeamRank(int ClientID, const char* pName);
	virtual void ShowPoints(int ClientID, const char* pName);
	virtual void ShowTimes(int ClientID, const char* pName, int Offset = 1);
	virtual void ShowTimes(int ClientID, int Offset = 1);
	virtual void ShowTop5(int ClientID, int Offset = 1);
	virtual void ShowTeamTop5(int ClientID, int Offset = 1);
	virtual void ShowTopPoints(int ClientID, int Offset = 1);
	virtual void GetSaves(int ClientID);

	// requested by teams
	virtual void SaveTeam(int ClientID, const char* Code, const char* Server);
	virtual void LoadTeam(const char* Code, int ClientID);

	// Game relevant not allowed to fail
	virtual void SaveScore(int ClientID, float Time, const char *pTimestamp,
			float CpTime[NUM_CHECKPOINTS], bool NotEligible);
	virtual void SaveTeamScore(int* aClientIDs, unsigned int Size, float Time, const char *pTimestamp);

	virtual void OnShutdown();
};

#endif // GAME_SERVER_SCORE_SQL_H
