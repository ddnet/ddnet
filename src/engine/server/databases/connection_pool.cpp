#include "connection_pool.h"

// helper struct to hold thread data
struct CSqlExecData
{
	CSqlExecData(
			bool (*pFuncPtr) (IDbConnection *, const ISqlData *),
			const ISqlData *pSqlRequestData
	);
	~CSqlExecData();

	bool (*m_pFuncPtr) (IDbConnection*, const ISqlData *);
	std::unique_ptr<const ISqlData> m_pSqlRequestData;
};

CDbConnectionPool::CDbConnectionPool()
{
}

CDbConnectionPool::~CDbConnectionPool()
{
}

void CDbConnectionPool::RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode)
{
}

void CDbConnectionPool::Execute(
		bool (*pFuncPtr) (IDbConnection *, const ISqlData *),
		std::unique_ptr<ISqlData> pSqlRequestData)
{
}

void CDbConnectionPool::ExecuteWrite(
		bool (*pFuncPtr) (IDbConnection *, const ISqlData *),
		std::unique_ptr<ISqlData> pSqlRequestData)
{
}

void CDbConnectionPool::Shutdown()
{
}
