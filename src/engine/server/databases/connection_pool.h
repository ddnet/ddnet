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
	virtual ~ISqlData() = default;

	mutable std::shared_ptr<ISqlResult> m_pResult;
};

enum Write
{
	// write everything into the backup db first
	BACKUP_FIRST,
	// now try to write it into remote db
	NORMAL,
	// succeeded writing -> remove copy from backup
	NORMAL_SUCCEEDED,
	// failed writing -> notify about failure
	NORMAL_FAILED,
};

class IConsole;

struct CMysqlConfig
{
	char m_aDatabase[64];
	char m_aPrefix[64];
	char m_aUser[64];
	char m_aPass[64];
	char m_aIp[64];
	char m_aBindaddr[128];
	int m_Port;
	bool m_Setup;
};

class CDbConnectionPool
{
public:
	CDbConnectionPool();
	~CDbConnectionPool();
	CDbConnectionPool &operator=(const CDbConnectionPool &) = delete;

	// Returns false on success.
	typedef bool (*FRead)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize);
	typedef bool (*FWrite)(IDbConnection *, const ISqlData *, Write, char *pError, int ErrorSize);

	enum Mode
	{
		READ,
		WRITE,
		WRITE_BACKUP,
		NUM_MODES,
	};

	void Print(IConsole *pConsole, Mode DatabaseMode);

	void RegisterSqliteDatabase(Mode DatabaseMode, const char FileName[64]);
	void RegisterMysqlDatabase(Mode DatabaseMode, const CMysqlConfig *pMysqlConfig);

	void Execute(
		FRead pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);
	// writes to WRITE_BACKUP first and removes it from there when successfully
	// executed on WRITE server
	void ExecuteWrite(
		FWrite pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);

	void OnShutdown();

	friend class CWorker;
	friend class CBackup;

private:
	static bool ExecSqlFunc(IDbConnection *pConnection, struct CSqlExecData *pData, Write w);

	// Only the main thread accesses this variable. It points to the index,
	// where the next query is added to the queue.
	int m_InsertIdx = 0;

	bool m_Shutdown = false;

	struct CSharedData
	{
		// Used as signal that shutdown is in progress from main thread to
		// speed up the queries by discarding read queries and writing to
		// the sqlite file instead of the remote mysql server.
		// The worker thread signals the main thread that all queries are
		// processed by setting this variable to false again.
		std::atomic_bool m_Shutdown{false};
		// Queries go first to the backup thread. This semaphore signals about
		// new queries.
		CSemaphore m_NumBackup;
		// When the backup thread processed the query, it signals the main
		// thread with this semaphore about the new query
		CSemaphore m_NumWorker;

		// spsc queue with additional backup worker to look at queries first.
		std::unique_ptr<struct CSqlExecData> m_aQueries[512];
	};

	std::shared_ptr<CSharedData> m_pShared;
	void *m_pWorkerThread = nullptr;
	void *m_pBackupThread = nullptr;
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
