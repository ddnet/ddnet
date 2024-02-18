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

	void LoadBestTime();
	void MapInfo(int ClientID, const char *pMapName);
	void MapVote(int ClientID, const char *pMapName);
	void LoadPlayerData(int ClientID, const char *pName = "");
	void SaveScore(int ClientID, float Time, const char *pTimestamp, const float aTimeCp[NUM_CHECKPOINTS], bool NotEligible);

	void SaveTeamScore(int *pClientIDs, unsigned int Size, float Time, const char *pTimestamp);

	void ShowTop(int ClientID, int Offset = 1);
	void ShowRank(int ClientID, const char *pName);

	void ShowTeamTop5(int ClientID, int Offset = 1);
	void ShowPlayerTeamTop5(int ClientID, const char *pName, int Offset = 1);
	void ShowTeamRank(int ClientID, const char *pName);

	void ShowTopPoints(int ClientID, int Offset = 1);
	void ShowPoints(int ClientID, const char *pName);

	void ShowTimes(int ClientID, const char *pName, int Offset = 1);
	void ShowTimes(int ClientID, int Offset = 1);

	void RandomMap(int ClientID, int Stars);
	void RandomUnfinishedMap(int ClientID, int Stars);

	void SaveTeam(int ClientID, const char *pCode, const char *pServer, bool Silent = false);
	void LoadTeam(const char *pCode, int ClientID);
	void GetSaves(int ClientID);
};

#endif // GAME_SERVER_SCORE_H
