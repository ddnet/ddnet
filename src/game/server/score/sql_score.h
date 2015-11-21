/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include "sqlserver.h"
#include "../score.h"


class CSqlScore: public IScore
{
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }
	CSqlServer *SqlServer() { return &m_SqlServer; }

	void Init();

	CGameContext *m_pGameServer;
	IServer *m_pServer;

	CSqlServer m_SqlServer;

	char m_aMap[64];

	static void MapInfoThread(void *pUser);
	static void MapVoteThread(void *pUser);
	static void LoadScoreThread(void *pUser);
	static void SaveScoreThread(void *pUser);
	static void SaveTeamScoreThread(void *pUser);
	static void ShowRankThread(void *pUser);
	static void ShowTop5Thread(void *pUser);
	static void ShowTeamRankThread(void *pUser);
	static void ShowTeamTop5Thread(void *pUser);
	static void ShowTimesThread(void *pUser);
	static void ShowPointsThread(void *pUser);
	static void ShowTopPointsThread(void *pUser);
	static void RandomMapThread(void *pUser);
	static void RandomUnfinishedMapThread(void *pUser);
	static void SaveTeamThread(void *pUser);
	static void LoadTeamThread(void *pUser);

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

// generic implementation to provide sqlserver, gameserver and server
struct CSqlData
{
	CSqlData(CSqlServer* pSqlServer) : m_pSqlServer(pSqlServer) {}

	CGameContext* GameServer() { return ms_pGameServer; }
	IServer* Server() { return ms_pServer; }
	CPlayerData* PlayerData(int ID) { return &ms_pPlayerData[ID]; }
	const char* MapName() { return ms_pMap; }
	CSqlServer* SqlServer() { return m_pSqlServer; }

	static CGameContext *ms_pGameServer;
	static IServer *ms_pServer;
	static CPlayerData *ms_pPlayerData;
	static const char *ms_pMap;
	CSqlServer *m_pSqlServer;
};

struct CSqlMapData : CSqlData
{
	CSqlMapData(CSqlServer* pSqlServer) : CSqlData(pSqlServer) {}

	int m_ClientID;
	char m_aMap[128];
};

struct CSqlScoreData : CSqlData
{
	CSqlScoreData(CSqlServer* pSqlServer) : CSqlData(pSqlServer) {}

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
	CSqlTeamScoreData(CSqlServer* pSqlServer) : CSqlData(pSqlServer) {}

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
	CSqlTeamSave(CSqlServer* pSqlServer) : CSqlData(pSqlServer) {}

	int m_Team;
	int m_ClientID;
	char m_Code[128];
	char m_Server[5];
};

struct CSqlTeamLoad : CSqlData
{
	CSqlTeamLoad(CSqlServer* pSqlServer) : CSqlData(pSqlServer) {}

	char m_Code[128];
	int m_ClientID;
};

#endif
