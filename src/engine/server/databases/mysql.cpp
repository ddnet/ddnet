#include "mysql.h"

#include <base/tl/threading.h>
#include <engine/console.h>
#if defined(CONF_SQL)
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#endif

#include <string>

lock CMysqlConnection::m_SqlDriverLock;

CMysqlConnection::CMysqlConnection(
	const char *pDatabase,
	const char *pPrefix,
	const char *pUser,
	const char *pPass,
	const char *pIp,
	int Port,
	bool Setup) :
	IDbConnection(pPrefix),
#if defined(CONF_SQL)
	m_NewQuery(false),
	m_Locked(false),
#endif
	m_Port(Port),
	m_Setup(Setup),
	m_InUse(false)
{
	str_copy(m_aDatabase, pDatabase, sizeof(m_aDatabase));
	str_copy(m_aUser, pUser, sizeof(m_aUser));
	str_copy(m_aPass, pPass, sizeof(m_aPass));
	str_copy(m_aIp, pIp, sizeof(m_aIp));
#ifndef CONF_SQL
	dbg_msg("sql", "Adding MySQL server failed due to MySQL support not enabled during compile time");
#endif
}

CMysqlConnection::~CMysqlConnection()
{
}

void CMysqlConnection::Print(IConsole *pConsole, const char *Mode)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"MySQL-%s: DB: '%s' Prefix: '%s' User: '%s' IP: <{'%s'}> Port: %d",
		Mode, m_aDatabase, GetPrefix(), m_aUser, m_aIp, m_Port);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

CMysqlConnection *CMysqlConnection::Copy()
{
	return new CMysqlConnection(m_aDatabase, GetPrefix(), m_aUser, m_aPass, m_aIp, m_Port, m_Setup);
}

void CMysqlConnection::ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize, "UNIX_TIMESTAMP(%s)", pTimestamp);
}

IDbConnection::Status CMysqlConnection::Connect()
{
#if defined(CONF_SQL)
	if(m_InUse.exchange(true))
		return Status::IN_USE;

	m_NewQuery = true;
	if(m_pConnection != nullptr)
	{
		try
		{
			// Connect to specific database
			m_pConnection->setSchema(m_aDatabase);
			return Status::SUCCESS;
		}
		catch(sql::SQLException &e)
		{
			dbg_msg("sql", "MySQL Error: %s", e.what());
		}
		catch(const std::exception &ex)
		{
			dbg_msg("sql", "MySQL Error: %s", ex.what());
		}
		catch(const std::string &ex)
		{
			dbg_msg("sql", "MySQL Error: %s", ex.c_str());
		}
		catch(...)
		{
			dbg_msg("sql", "Unknown Error cause by the MySQL/C++ Connector");
		}

		dbg_msg("sql", "FAILURE: SQL connection failed, trying to reconnect");
	}

	try
	{
		m_pConnection.reset();
		m_pPreparedStmt.reset();
		m_pResults.reset();

		sql::ConnectOptionsMap connection_properties;
		connection_properties["hostName"] = sql::SQLString(m_aIp);
		connection_properties["port"] = m_Port;
		connection_properties["userName"] = sql::SQLString(m_aUser);
		connection_properties["password"] = sql::SQLString(m_aPass);
		connection_properties["OPT_CONNECT_TIMEOUT"] = 60;
		connection_properties["OPT_READ_TIMEOUT"] = 60;
		connection_properties["OPT_WRITE_TIMEOUT"] = 120;
		connection_properties["OPT_RECONNECT"] = true;
		connection_properties["OPT_CHARSET_NAME"] = sql::SQLString("utf8mb4");
		connection_properties["OPT_SET_CHARSET_NAME"] = sql::SQLString("utf8mb4");

		// Create connection
		{
			scope_lock GlobalLockScope(&m_SqlDriverLock);
			sql::Driver *pDriver = get_driver_instance();
			m_pConnection.reset(pDriver->connect(connection_properties));
		}

		// Create Statement
		m_pStmt = std::unique_ptr<sql::Statement>(m_pConnection->createStatement());

		// Apparently OPT_CHARSET_NAME and OPT_SET_CHARSET_NAME are not enough
		m_pStmt->execute("SET CHARACTER SET utf8mb4;");

		if(m_Setup)
		{
			char aBuf[1024];
			// create database
			str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s CHARACTER SET utf8mb4", m_aDatabase);
			m_pStmt->execute(aBuf);
			// Connect to specific database
			m_pConnection->setSchema(m_aDatabase);
			FormatCreateRace(aBuf, sizeof(aBuf));
			m_pStmt->execute(aBuf);
			FormatCreateTeamrace(aBuf, sizeof(aBuf), "VARBINARY(16)");
			m_pStmt->execute(aBuf);
			FormatCreateMaps(aBuf, sizeof(aBuf));
			m_pStmt->execute(aBuf);
			FormatCreateSaves(aBuf, sizeof(aBuf));
			m_pStmt->execute(aBuf);
			FormatCreatePoints(aBuf, sizeof(aBuf));
			m_pStmt->execute(aBuf);
			m_Setup = false;
		}
		else
		{
			// Connect to specific database
			m_pConnection->setSchema(m_aDatabase);
		}
		dbg_msg("sql", "sql connection established");
		return Status::SUCCESS;
	}
	catch(sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
	}
	catch(const std::exception &ex)
	{
		dbg_msg("sql", "MySQL Error: %s", ex.what());
	}
	catch(const std::string &ex)
	{
		dbg_msg("sql", "MySQL Error: %s", ex.c_str());
	}
	catch(...)
	{
		dbg_msg("sql", "Unknown Error cause by the MySQL/C++ Connector");
	}
	m_InUse.store(false);

#endif
	dbg_msg("sql", "FAILURE: sql connection failed");
	return Status::FAILURE;
}

void CMysqlConnection::Disconnect()
{
	m_InUse.store(false);
}

void CMysqlConnection::Lock(const char *pTable)
{
#if defined(CONF_SQL)
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "LOCK TABLES %s;", pTable);
	m_pStmt->execute(aBuf);
	m_Locked = true;
#endif
}

void CMysqlConnection::Unlock()
{
#if defined(CONF_SQL)
	if(m_Locked)
	{
		m_pStmt->execute("UNLOCK TABLES;");
		m_Locked = false;
	}
#endif
}

void CMysqlConnection::PrepareStatement(const char *pStmt)
{
#if defined(CONF_SQL)
	m_pPreparedStmt.reset(m_pConnection->prepareStatement(pStmt));
	m_NewQuery = true;
#endif
}

void CMysqlConnection::BindString(int Idx, const char *pString)
{
#if defined(CONF_SQL)
	m_pPreparedStmt->setString(Idx, pString);
	m_NewQuery = true;
#endif
}

void CMysqlConnection::BindBlob(int Idx, unsigned char *pBlob, int Size)
{
#if defined(CONF_SQL)
	// copy blob into string
	auto Blob = std::string(pBlob, pBlob + Size);
	m_pPreparedStmt->setString(Idx, Blob);
	m_NewQuery = true;
#endif
}

void CMysqlConnection::BindInt(int Idx, int Value)
{
#if defined(CONF_SQL)
	m_pPreparedStmt->setInt(Idx, Value);
	m_NewQuery = true;
#endif
}

void CMysqlConnection::BindFloat(int Idx, float Value)
{
#if defined(CONF_SQL)
	m_pPreparedStmt->setDouble(Idx, (double)Value);
	m_NewQuery = true;
#endif
}

bool CMysqlConnection::Step()
{
#if defined(CONF_SQL)
	if(m_NewQuery)
	{
		m_NewQuery = false;
		m_pResults.reset(m_pPreparedStmt->executeQuery());
	}
	return m_pResults->next();
#else
	return false;
#endif
}

bool CMysqlConnection::IsNull(int Col) const
{
#if defined(CONF_SQL)
	return m_pResults->isNull(Col);
#else
	return false;
#endif
}

float CMysqlConnection::GetFloat(int Col) const
{
#if defined(CONF_SQL)
	return (float)m_pResults->getDouble(Col);
#else
	return 0.0;
#endif
}

int CMysqlConnection::GetInt(int Col) const
{
#if defined(CONF_SQL)
	return m_pResults->getInt(Col);
#else
	return 0;
#endif
}

void CMysqlConnection::GetString(int Col, char *pBuffer, int BufferSize) const
{
#if defined(CONF_SQL)
	auto String = m_pResults->getString(Col);
	str_copy(pBuffer, String.c_str(), BufferSize);
#endif
}

int CMysqlConnection::GetBlob(int Col, unsigned char *pBuffer, int BufferSize) const
{
#if defined(CONF_SQL)
	auto *Blob = m_pResults->getBlob(Col);
	Blob->read((char *)pBuffer, BufferSize);
	int NumRead = Blob->gcount();
	delete Blob;
	return NumRead;
#else
	return 0;
#endif
}

void CMysqlConnection::AddPoints(const char *pPlayer, int Points)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"INSERT INTO %s_points(Name, Points) "
		"VALUES (?, ?) "
		"ON DUPLICATE KEY UPDATE Points=Points+?;",
		GetPrefix());
	PrepareStatement(aBuf);
	BindString(1, pPlayer);
	BindInt(2, Points);
	BindInt(3, Points);
	Step();
}
