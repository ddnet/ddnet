#ifndef GAME_SERVER_INSTAGIB_SQL_STATS_H
#define GAME_SERVER_INSTAGIB_SQL_STATS_H

#include <engine/server/databases/connection_pool.h>
#include <game/server/instagib/extra_columns.h>
#include <game/server/instagib/sql_stats_player.h>

struct ISqlData;
class IDbConnection;
class IServer;
class CGameContext;

struct CSqlInstaData : ISqlData
{
	CSqlInstaData() :
		ISqlData(nullptr)
	{
	}

	~CSqlInstaData() override;

	CExtraColumns *m_pExtraColumns = nullptr;
};

struct CSqlSaveRoundStatsRequest : CSqlInstaData
{
	char m_aName[128];
	char m_aTable[128];
	CSqlStatsPlayer m_Stats;
};

struct CSqlCreateTableRequest : ISqlData
{
	CSqlCreateTableRequest() :
		ISqlData(nullptr)
	{
	}
	char m_aName[128];
	char m_aColumns[2048];
};

class CSqlStats
{
	CDbConnectionPool *m_pPool;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }
	CGameContext *m_pGameServer;
	IServer *m_pServer;

	CExtraColumns *m_pExtraColumns = nullptr;

	// non ratelimited server side queries
	static bool CreateTableThread(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);
	static bool SaveRoundStatsThread(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);

public:
	CSqlStats(CGameContext *pGameServer, CDbConnectionPool *pPool);
	~CSqlStats() = default;

	void SetExtraColumns(CExtraColumns *pExtraColumns);

	void CreateTable(const char *pName);
	void SaveRoundStats(const char *pName, const char *pTable, CSqlStatsPlayer *pStats);
};

#endif
