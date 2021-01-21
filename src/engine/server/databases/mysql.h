#ifndef ENGINE_SERVER_DATABASES_MYSQL_H
#define ENGINE_SERVER_DATABASES_MYSQL_H

#include <atomic>
#include <engine/server/databases/connection.h>
#include <memory>

class CLock;
namespace sql {
class Connection;
class PreparedStatement;
class ResultSet;
class Statement;
} /* namespace sql */

class CMysqlConnection : public IDbConnection
{
public:
	CMysqlConnection(
		const char *pDatabase,
		const char *pPrefix,
		const char *pUser,
		const char *pPass,
		const char *pIp,
		int Port,
		bool Setup);
	virtual ~CMysqlConnection();
	virtual void Print(IConsole *pConsole, const char *Mode);

	virtual CMysqlConnection *Copy();

	virtual const char *BinaryCollate() const { return "utf8mb4_bin"; }
	virtual void ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize);
	virtual const char *InsertTimestampAsUtc() const { return "?"; }
	virtual const char *CollateNocase() const { return "CONVERT(? USING utf8mb4) COLLATE utf8mb4_general_ci"; }
	virtual const char *InsertIgnore() const { return "INSERT IGNORE"; };
	virtual const char *Random() const { return "RAND()"; };
	virtual const char *MedianMapTime(char *pBuffer, int BufferSize) const;

	virtual Status Connect();
	virtual void Disconnect();

	virtual void PrepareStatement(const char *pStmt);

	virtual void BindString(int Idx, const char *pString);
	virtual void BindBlob(int Idx, unsigned char *pBlob, int Size);
	virtual void BindInt(int Idx, int Value);
	virtual void BindFloat(int Idx, float Value);

	virtual void Print() {}
	virtual bool Step();
	virtual int ExecuteUpdate();

	virtual bool IsNull(int Col) const;
	virtual float GetFloat(int Col) const;
	virtual int GetInt(int Col) const;
	virtual void GetString(int Col, char *pBuffer, int BufferSize) const;
	virtual int GetBlob(int Col, unsigned char *pBuffer, int BufferSize) const;

	virtual void AddPoints(const char *pPlayer, int Points);

private:
#if defined(CONF_SQL)
	std::unique_ptr<sql::Connection> m_pConnection;
	std::unique_ptr<sql::PreparedStatement> m_pPreparedStmt;
	std::unique_ptr<sql::Statement> m_pStmt;
	std::unique_ptr<sql::ResultSet> m_pResults;
	bool m_NewQuery;
#endif

	// copy of config vars
	char m_aDatabase[64];
	char m_aUser[64];
	char m_aPass[64];
	char m_aIp[64];
	int m_Port;
	bool m_Setup;

	std::atomic_bool m_InUse;
	static CLock m_SqlDriverLock;
};

#endif // ENGINE_SERVER_DATABASES_MYSQL_H
