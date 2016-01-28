#if defined(CONF_SQL)

#include <base/system.h>

#include "sql_connector.h"

CSqlServer** CSqlConnector::ms_ppSqlReadServers = 0;
CSqlServer** CSqlConnector::ms_ppSqlWriteServers = 0;

int CSqlConnector::ms_ReachableReadServer = 0;
int CSqlConnector::ms_ReachableWriteServer = 0;

CSqlConnector::CSqlConnector() :
m_pSqlServer(0),
m_NumReadRetries(0),
m_NumWriteRetries(0)
{}

bool CSqlConnector::ConnectSqlServer(bool ReadOnly)
{
	ReadOnly ? ++m_NumReadRetries : ++m_NumWriteRetries;
	int& ReachableServer = ReadOnly ? ms_ReachableReadServer : ms_ReachableWriteServer;
	int NumServers = ReadOnly ? CSqlServer::ms_NumReadServer : CSqlServer::ms_NumWriteServer;

	for (int i = ReachableServer, ID = ReachableServer; i < ReachableServer + NumServers && SqlServer(i % NumServers, ReadOnly); i++, ID = i % NumServers)
	{
		if (SqlServer(ID, ReadOnly) && SqlServer(ID, ReadOnly)->Connect())
		{
			m_pSqlServer = SqlServer(ID, ReadOnly);
			ReachableServer = ID;
			return true;
		}
		if (SqlServer(ID, ReadOnly))
			dbg_msg("SQL", "Warning: Unable to connect to Sql%sServer %d ('%s'), trying next...", ReadOnly ? "Read" : "Write", ID, SqlServer(ID, ReadOnly)->GetIP());
	}
	dbg_msg("SQL", "FATAL ERROR: No Sql%sServers available", ReadOnly ? "Read" : "Write");
	m_pSqlServer = 0;
	return false;
}

#endif
