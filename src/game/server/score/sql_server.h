#ifndef GAME_SERVER_SQL_SERVER_H
#define GAME_SERVER_SQL_SERVER_H

#include <mysql_connection.h>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

class CSqlServer
{
public:
	CSqlServer(const char* pDatabase, const char* pPrefix, const char* pUser, const char* pPass, const char* pIp, int Port);
	~CSqlServer();

	bool Connect();
	void Disconnect();
	void CreateTables();

	void executeSql(const char* pCommand);
	void executeSqlQuery(const char* pQuery);
	sql::ResultSet* GetResults();

	const char* GetPrefix();

private:
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
	int m_Port;
};

#endif
