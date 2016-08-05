#ifndef TOOLS_RESTORE_RANKS_H
#define TOOLS_RESTORE_RANKS_H

#include <base/system.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/storage.h>


#include <engine/shared/config.h>
#include <engine/shared/sql_server.h>
#include <engine/shared/sql_connector.h>

#include <game/server/score/sql_data.h>

/*
ExecSqlFunc:

	Basically the same as defined in game/server/score/sql_score.h except that it is typesafe and
	does not delete the data supplied.
*/
template <typename ... Types>
void ExecSqlFunc(bool (*pFuncPtr) (bool, CSqlServer*, Types...), bool readOnly, Types ... args)
{
	CSqlConnector connector;

	bool Success = false;

	// try to connect to a working databaseserver
	while (!Success && !connector.MaxTriesReached(readOnly) && connector.ConnectSqlServer(readOnly))
	{
		if (pFuncPtr(false, connector.SqlServer(), args...))
			Success = true;

		// disconnect from databaseserver
		connector.SqlServer()->Disconnect();
	}

	// handle failures
	// eg write inserts to a file and print a nice error message
	if (!Success)
		pFuncPtr(true, nullptr, args...);
}

class CRestoreRanks
{
public:
    CRestoreRanks(IConsole* pConsolestatic , IConfig* pConfig);
	~CRestoreRanks();

    void parseJSON();
	static bool SaveScore(bool HandleFailure, CSqlServer* pSqlServer, const CSqlScoreData& pData);
	static bool SaveTeamScore(bool HandleFailure, CSqlServer* pSqlServer, const CSqlTeamScoreData& Data);

    IConsole* Console() {return m_pConsole; }
    IConfig* Config() {return m_pConfig; }

private:
    IConsole* m_pConsole;
    IConfig* m_pConfig;

	// g_Config.m_SvSqlFailureFile + ".tmp" --> 4 chars longer
	char m_FailureFileTmp[sizeof(g_Config.m_SvSqlFailureFile) + 4];
};

#endif
