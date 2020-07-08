#include "connection_pool.h"

#if defined(CONF_SQL)
#include <cppconn/exception.h>
#endif
#include <stdexcept>

// helper struct to hold thread data
struct CSqlExecData
{
	CSqlExecData(
			CDbConnectionPool::FRead pFunc,
			std::unique_ptr<const ISqlData> pThreadData,
			const char *pName);
	CSqlExecData(
			CDbConnectionPool::FWrite pFunc,
			std::unique_ptr<const ISqlData> pThreadData,
			const char *pName);
	~CSqlExecData() {}

	enum
	{
		READ_ACCESS,
		WRITE_ACCESS,
	} m_Mode;
	union
	{
		CDbConnectionPool::FRead m_pReadFunc;
		CDbConnectionPool::FWrite m_pWriteFunc;
	} m_Ptr;

	std::unique_ptr<const ISqlData> m_pThreadData;
	const char *m_pName;
};

CSqlExecData::CSqlExecData(
	CDbConnectionPool::FRead pFunc,
	std::unique_ptr<const ISqlData> pThreadData,
	const char *pName) :
			m_Mode(READ_ACCESS),
			m_pThreadData(std::move(pThreadData)),
			m_pName(pName)
{
	m_Ptr.m_pReadFunc = pFunc;
}

CSqlExecData::CSqlExecData(
	CDbConnectionPool::FWrite pFunc,
	std::unique_ptr<const ISqlData> pThreadData,
	const char *pName) :
			m_Mode(WRITE_ACCESS),
			m_pThreadData(std::move(pThreadData)),
			m_pName(pName)
{
	m_Ptr.m_pWriteFunc = pFunc;
}

CDbConnectionPool::CDbConnectionPool() :
	m_NumElem(),
	FirstElem(0),
	LastElem(0)
{
	thread_init_and_detach(CDbConnectionPool::Worker, this, "database worker thread");
}

CDbConnectionPool::~CDbConnectionPool()
{
}

void CDbConnectionPool::RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode)
{
	if(DatabaseMode < 0 || NUM_MODES <= DatabaseMode)
		return;
	m_aapDbConnections[DatabaseMode].push_back(std::move(pDatabase));
}

void CDbConnectionPool::Execute(
		FRead pFunc,
		std::unique_ptr<const ISqlData> pThreadData,
		const char *pName)
{
	m_aTasks[FirstElem++].reset(new CSqlExecData(pFunc, std::move(pThreadData), pName));
	FirstElem %= sizeof(m_aTasks) / sizeof(m_aTasks[0]);
	m_NumElem.signal();
}

void CDbConnectionPool::ExecuteWrite(
		FWrite pFunc,
		std::unique_ptr<const ISqlData> pThreadData,
		const char *pName)
{
	m_aTasks[FirstElem++].reset(new CSqlExecData(pFunc, std::move(pThreadData), pName));
	FirstElem %= sizeof(m_aTasks) / sizeof(m_aTasks[0]);
	m_NumElem.signal();
}

void CDbConnectionPool::OnShutdown()
{
}

void CDbConnectionPool::Worker(void *pUser)
{
	CDbConnectionPool *pThis = (CDbConnectionPool *)pUser;
	pThis->Worker();
}

void CDbConnectionPool::Worker()
{
	while(1)
	{
		m_NumElem.wait();
		auto pThreadData = std::move(m_aTasks[LastElem++]);
		LastElem %= sizeof(m_aTasks) / sizeof(m_aTasks[0]);
		bool Success = false;
		switch(pThreadData->m_Mode)
		{
		case CSqlExecData::READ_ACCESS:
		{
			for(int i = 0; i < (int)m_aapDbConnections[Mode::READ].size(); i++)
			{
				if(ExecSqlFunc(m_aapDbConnections[Mode::READ][i].get(), pThreadData.get(), false))
				{
					Success = true;
					break;
				}
			}
		} break;
		case CSqlExecData::WRITE_ACCESS:
		{
			for(int i = 0; i < (int)m_aapDbConnections[Mode::WRITE].size(); i++)
			{
				if(ExecSqlFunc(m_aapDbConnections[Mode::WRITE][i].get(), pThreadData.get(), false))
				{
					Success = true;
					break;
				}
			}
			if(!Success)
			{
				for(int i = 0; i < (int)m_aapDbConnections[Mode::WRITE_BACKUP].size(); i++)
				{
					if(ExecSqlFunc(m_aapDbConnections[Mode::WRITE_BACKUP][i].get(), pThreadData.get(), true))
					{
						Success = true;
						break;
					}
				}
			}
		} break;
		}
		if(Success)
			dbg_msg("sql", "%s done", pThreadData->m_pName);
	}
}

bool CDbConnectionPool::ExecSqlFunc(IDbConnection *pConnection, CSqlExecData *pData, bool Failure)
{
	if(pConnection->Connect() != IDbConnection::SUCCESS)
		return false;
	bool Success = false;
	try
	{
		switch(pData->m_Mode)
		{
		case CSqlExecData::READ_ACCESS:
			if(pData->m_Ptr.m_pReadFunc(pConnection, pData->m_pThreadData.get()))
				Success = true;
			break;
		case CSqlExecData::WRITE_ACCESS:
			if(pData->m_Ptr.m_pWriteFunc(pConnection, pData->m_pThreadData.get(), Failure))
				Success = true;
			break;
		}
	}
#if defined(CONF_SQL)
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
	}
#endif
	catch (std::runtime_error &e)
	{
		dbg_msg("sql", "SQLite Error: %s", e.what());
	}
	catch (...)
	{
		dbg_msg("sql", "Unexpected exception caught");
	}
	pConnection->Unlock();
	pConnection->Disconnect();
	if(!Success)
		dbg_msg("sql", "%s failed", pData->m_pName);
	return Success;
}

