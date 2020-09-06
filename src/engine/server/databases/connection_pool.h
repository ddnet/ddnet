#ifndef ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
#define ENGINE_SERVER_DATABASES_CONNECTION_POOL_H

#include <memory>

class IDbConnection;
class IConsole;

struct ISqlData
{
	virtual ~ISqlData() {};
};

typedef bool (*FSqlThread)(IDbConnection *, const ISqlData *, bool);

struct CSqlResponse
{
	enum Type
	{
		NONE,
		CONSOLE,
	} m_Type;
	char aMsg[122];

	CSqlResponse()
		: CSqlResponse(NONE)
	{
	}
	CSqlResponse(Type t)
		: m_Type(t),
		  aMsg{0}
	{
	}
};

class CDbConnectionPool
{
public:
	CDbConnectionPool();
	~CDbConnectionPool();
	CDbConnectionPool& operator=(const CDbConnectionPool&) = delete;

	enum Mode
	{
		READ,
		WRITE,
		WRITE_BACKUP,
		NUM_MODES,
	};

	void Print(IConsole *pConsole, Mode DatabaseMode);

	void RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode);

	void ExecuteRead(
		FSqlThread pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);
	// writes to WRITE_BACKUP server in case of failure
	void ExecuteWrite(
		FSqlThread pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);
	// retries writes to main WRITE server multiple times and WRITE_BACKUP server in case of failure
	void ExecuteWriteFaultTolerant(
		FSqlThread pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);

	// Tries to fetch response, returns true if successful
	bool GetResponse(CSqlResponse *pResponse);

	void OnShutdown();

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
