#ifndef ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
#define ENGINE_SERVER_DATABASES_CONNECTION_POOL_H

#include "connection.h"

#include <base/tl/threading.h>
#include <memory>
#include <vector>

struct ISqlData
{
	virtual ~ISqlData();
};

class CDbConnectionPool
{
public:
	CDbConnectionPool();
	~CDbConnectionPool();

	enum Mode
	{
		READ,
		WRITE,
		WRITE_BACKUP,
		NUM_MODES,
	};

	void RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode);

	void Execute(
			bool (*pFuncPtr) (IDbConnection *, const ISqlData *),
			std::unique_ptr<ISqlData> pSqlRequestData);
	// writes to WRITE_BACKUP server in case of failure
	void ExecuteWrite(
			bool (*pFuncPtr) (IDbConnection *, const ISqlData *),
			std::unique_ptr<ISqlData> pSqlRequestData);

	void Shutdown();

private:
	std::vector<std::unique_ptr<IDbConnection>> m_aapDbConnections[NUM_MODES];
	lock m_ConnectionLookupLock[NUM_MODES];
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
