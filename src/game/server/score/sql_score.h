/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include <mysql_connection.h>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include "../score.h"

class CSqlScore : public IScore
{
	CGameContext *m_pGameServer;
	IServer *m_pServer;

	sql::Driver *m_pDriver;
	sql::Connection *m_pConnection;
	sql::Statement *m_pStatement;
	sql::ResultSet *m_pResults;

	// copy of config vars
	const char* m_pDatabase;
	const char* m_pPrefix;
	const char* m_pUser;
	const char* m_pPass;
	const char* m_pIp;
	char m_aMap[64];
	int m_Port;

	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	static void LoadScoreThread(void *pUser);
	static void SaveScoreThread(void *pUser);
	static void ShowRankThread(void *pUser);
	static void ShowTop5Thread(void *pUser);
	static void ShowTimesThread(void *pUser);	

	void Init();

	bool Connect();
	void Disconnect();

	// anti SQL injection
	void ClearString(char *pString);

	void NormalizeMapname(char *pString);

public:

	CSqlScore(CGameContext *pGameServer);
	~CSqlScore();

	virtual void LoadScore(int ClientID);
	virtual void SaveScore(int ClientID, float Time, CCharacter *pChar);
	virtual void ShowRank(int ClientID, const char* pName, bool Search=false);
	virtual void ShowTimes(int ClientID, const char* pName, int Debut=1);
	virtual void ShowTimes(int ClientID, int Debut=1);	
	virtual void ShowTop5(IConsole::IResult *pResult, int ClientID, int Debut=1);
 	static void agoTimeToString(int agoTime, char agoStrign[]);
};

struct CSqlScoreData
{
	CSqlScore *m_pSqlData;
	int m_ClientID;
#if defined(CONF_FAMILY_WINDOWS)
	char m_aName[16]; // Don't edit this, or all your teeth will fall http://bugs.mysql.com/bug.php?id=50046
#else
	char m_aName[MAX_NAME_LENGTH*2-1];
#endif

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

#endif
