#ifndef ENGINE_SERVER_SQL_CONNECTOR_H
#define ENGINE_SERVER_SQL_CONNECTOR_H

#include "sql_server.h"

enum
{
	MAX_SQLSERVERS=15
};

// implementation to provide sqlservers
class CSqlConnector
{
public:
	CSqlConnector();

	CSqlServer* SqlServer(int i, bool ReadOnly = true) { return ReadOnly ? ms_ppSqlReadServers[i] : ms_ppSqlWriteServers[i]; }

	// always returns the last connected sql-server
	CSqlServer* SqlServer() { return m_pSqlServer; }

	static void SetReadServers(CSqlServer** ppReadServers) { ms_ppSqlReadServers = ppReadServers; }
	static void SetWriteServers(CSqlServer** ppWriteServers) { ms_ppSqlWriteServers = ppWriteServers; }

	bool ConnectSqlServer(bool ReadOnly = true);

	bool MaxTriesReached(bool ReadOnly = true) { return ReadOnly ? m_NumReadRetries >= CSqlServer::ms_NumReadServer : m_NumWriteRetries >= CSqlServer::ms_NumWriteServer; }
	
private:

	CSqlServer *m_pSqlServer;
	static CSqlServer **ms_ppSqlReadServers;
	static CSqlServer **ms_ppSqlWriteServers;

	static int ms_NumReadServer;
	static int ms_NumWriteServer;

	static int ms_ReachableReadServer;
	static int ms_ReachableWriteServer;

	int m_NumReadRetries;
	int m_NumWriteRetries;
};

#endif
