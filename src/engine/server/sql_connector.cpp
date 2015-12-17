#if defined(CONF_SQL)

#include <base/system.h>

#include "sql_connector.h"

CSqlConnector::CSqlConnector() :
m_pSqlServer(0),
m_NumReadRetries(0),
m_NumWriteRetries(0)
{}

bool CSqlConnector::ConnectSqlServer(bool ReadOnly)
{
	ReadOnly ? ++m_NumReadRetries : ++m_NumWriteRetries;

	bool passedOldServer = !m_pSqlServer;

	for (int i = 0; i < MAX_SQLSERVERS && SqlServer(i, ReadOnly); i++)
	{

		if (!passedOldServer)
		{
			if (m_pSqlServer == SqlServer(i, ReadOnly))
					passedOldServer = true;
			continue;
		}

		if (SqlServer(i, ReadOnly) && SqlServer(i, ReadOnly)->Connect())
		{
			m_pSqlServer = SqlServer(i, ReadOnly);
			return true;
		}
		if (SqlServer())
			dbg_msg("SQL", "Warning: Unable to connect to Sql%sServer %d ('%s'), trying next...", ReadOnly ? "read" : "write", i, SqlServer()->GetIP());
	}
	dbg_msg("SQL", "ERROR: No Sql%sServers available", ReadOnly ? "Read" : "Write");
	m_pSqlServer = 0;
	return false;
}

#endif
