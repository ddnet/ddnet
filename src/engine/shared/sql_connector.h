#ifndef ENGINE_SHARED_SQL_CONNECTOR_H
#define ENGINE_SHARED_SQL_CONNECTOR_H

#include "sql_server.h"

// implementation to provide sqlservers
class CSqlConnector
{
public:
	CSqlConnector();

	CSqlServer* SqlServer(int i, bool ReadOnly = true) { return ReadOnly ? CSqlServer::ms_apSqlReadServers[i] : CSqlServer::ms_apSqlWriteServers[i]; }

	// always returns the last connected sql-server
	CSqlServer* SqlServer() { return m_pSqlServer; }

	static void ResetReachable() { ms_ReachableReadServer = 0; ms_ReachableWriteServer = 0; }

	bool ConnectSqlServer(bool ReadOnly = true);

	bool MaxTriesReached(bool ReadOnly = true) { return ReadOnly ? m_NumReadRetries >= CSqlServer::ms_NumReadServer : m_NumWriteRetries >= CSqlServer::ms_NumWriteServer; }

private:

	CSqlServer *m_pSqlServer;

	static int ms_ReachableReadServer;
	static int ms_ReachableWriteServer;

	int m_NumReadRetries;
	int m_NumWriteRetries;
};

#endif
