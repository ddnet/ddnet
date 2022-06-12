#ifndef ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
#define ENGINE_SERVER_DATABASES_CONNECTION_POOL_H

#include <atomic>
#include <base/tl/threading.h>
#include <memory>
#include <vector>

class IDbConnection;

struct ISqlResult
{
	// using atomic_bool to indicate completed sql query since usage_count
	// from shard_ptr isn't safe in multithreaded environment
	// the main thread must only access the remaining result data if set to true
	std::atomic_bool m_Completed{false};
	// indicate whether the thread indicated a successful completion (returned true)
	bool m_Success = false;

	virtual ~ISqlResult() = default;
};

struct ISqlData
{
	ISqlData(std::shared_ptr<ISqlResult> pResult) :
		m_pResult(std::move(pResult))
	{
	}
	virtual ~ISqlData(){};

	mutable std::shared_ptr<ISqlResult> m_pResult;
};

class IConsole;

class CDbConnectionPool
{
public:
	CDbConnectionPool();
	~CDbConnectionPool();
	CDbConnectionPool &operator=(const CDbConnectionPool &) = delete;

	// Returns false on success.
	typedef bool (*FRead)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize);
	typedef bool (*FWrite)(IDbConnection *, const ISqlData *, bool, char *pError, int ErrorSize);

	enum Mode
	{
		READ,
		WRITE,
		WRITE_BACKUP,
		NUM_MODES,
	};

	void Print(IConsole *pConsole, Mode DatabaseMode);

	void RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode);

	void Execute(
		FRead pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);
	// writes to WRITE_BACKUP server in case of failure
	void ExecuteWrite(
		FWrite pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);

	void OnShutdown();

private:
	std::vector<std::unique_ptr<IDbConnection>> m_vvpDbConnections[NUM_MODES];

	static void Worker(void *pUser);
	void Worker();
	bool ExecSqlFunc(IDbConnection *pConnection, struct CSqlExecData *pData, bool Failure);

	std::atomic_bool m_Shutdown{false};
	CSemaphore m_NumElem;
	int m_FirstElem;
	int m_LastElem;
	std::unique_ptr<struct CSqlExecData> m_aTasks[512];
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
