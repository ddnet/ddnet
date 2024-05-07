#ifndef GAME_SERVER_SCORE_H
#define GAME_SERVER_SCORE_H

#include <game/prng.h>

#include "scoreworker.h"

class CDbConnectionPool;
class CGameContext;
class IDbConnection;
class IServer;
struct ISqlData;

class CScore
{
	CPlayerData m_aPlayerData[MAX_CLIENTS];
	CDbConnectionPool *m_pPool;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }
	CGameContext *m_pGameServer;
	IServer *m_pServer;

	std::vector<std::string> m_vWordlist;
	CPrng m_Prng;
	void GeneratePassphrase(char *pBuf, int BufSize);

	// returns new SqlResult bound to the player, if no current Thread is active for this player
	std::shared_ptr<CScorePlayerResult> NewSqlPlayerResult(int ClientId);
	// Creates for player database requests
	void ExecPlayerThread(
		bool (*pFuncPtr)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize),
		const char *pThreadName,
		int ClientId,
		const char *pName,
		int Offset);

	// returns true if the player should be rate limited
	bool RateLimitPlayer(int ClientId);

public:
	CScore(CGameContext *pGameServer, CDbConnectionPool *pPool);
	~CScore() {}

	CPlayerData *PlayerData(int Id) { return &m_aPlayerData[Id]; }

	void LoadBestTime();
	void MapInfo(int ClientId, const char *pMapName);
	void MapVote(int ClientId, const char *pMapName);
	void LoadPlayerData(int ClientId, const char *pName = "");
	void LoadPlayerTimeCp(int ClientId, const char *pName = "");
	void SaveScore(int ClientId, int TimeTicks, const char *pTimestamp, const float aTimeCp[NUM_CHECKPOINTS], bool NotEligible);

	void SaveTeamScore(int Team, int *pClientIds, unsigned int Size, int TimeTicks, const char *pTimestamp);

	void ShowTop(int ClientId, int Offset = 1);
	void ShowRank(int ClientId, const char *pName);

	void ShowTeamTop5(int ClientId, int Offset = 1);
	void ShowPlayerTeamTop5(int ClientId, const char *pName, int Offset = 1);
	void ShowTeamRank(int ClientId, const char *pName);

	void ShowTopPoints(int ClientId, int Offset = 1);
	void ShowPoints(int ClientId, const char *pName);

	void ShowTimes(int ClientId, const char *pName, int Offset = 1);
	void ShowTimes(int ClientId, int Offset = 1);

	void RandomMap(int ClientId, int Stars);
	void RandomUnfinishedMap(int ClientId, int Stars);

	void SaveTeam(int ClientId, const char *pCode, const char *pServer);
	void LoadTeam(const char *pCode, int ClientId);
	void GetSaves(int ClientId);
};

#endif // GAME_SERVER_SCORE_H
