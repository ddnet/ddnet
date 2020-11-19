#include "connection_pool.h"
#include "connection.h"

#include <engine/console.h>
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

void CDbConnectionPool::Print(IConsole *pConsole, Mode DatabaseMode)
{
	const char *ModeDesc[] = {"Read", "Write", "WriteBackup"};
	for(unsigned int i = 0; i < m_aapDbConnections[DatabaseMode].size(); i++)
	{
		m_aapDbConnections[DatabaseMode][i]->Print(pConsole, ModeDesc[DatabaseMode]);
	}
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
	m_Shutdown.store(true);
	m_NumElem.signal();
	int i = 0;
	while(m_Shutdown.load())
	{
		if(i > 600)
		{
			dbg_msg("sql", "Waited 60 seconds for score-threads to complete, quitting anyway");
			break;
		}

		// print a log about every two seconds
		if(i % 20 == 0)
			dbg_msg("sql", "Waiting for score-threads to complete (%ds)", i / 10);
		++i;
		thread_sleep(100000);
	}
}

void CDbConnectionPool::Worker(void *pUser)
{
	CDbConnectionPool *pThis = (CDbConnectionPool *)pUser;
	pThis->Worker();
}

void CDbConnectionPool::Worker()
{
	// remember last working server and try to connect to it first
	int ReadServer = 0;
	int WriteServer = 0;
	while(1)
	{
		m_NumElem.wait();
		auto pThreadData = std::move(m_aTasks[LastElem++]);
		// work through all database jobs after OnShutdown is called before exiting the thread
		if(pThreadData == nullptr)
		{
			m_Shutdown.store(false);
			return;
		}
		LastElem %= sizeof(m_aTasks) / sizeof(m_aTasks[0]);
		bool Success = false;
		switch(pThreadData->m_Mode)
		{
		case CSqlExecData::READ_ACCESS:
		{
			for(int i = 0; i < (int)m_aapDbConnections[Mode::READ].size(); i++)
			{
				int CurServer = (ReadServer + i) % (int)m_aapDbConnections[Mode::READ].size();
				if(ExecSqlFunc(m_aapDbConnections[Mode::READ][CurServer].get(), pThreadData.get(), false))
				{
					ReadServer = CurServer;
					dbg_msg("sql", "%s done on read database %d", pThreadData->m_pName, CurServer);
					Success = true;
					break;
				}
			}
		}
		break;
		case CSqlExecData::WRITE_ACCESS:
		{
			for(int i = 0; i < (int)m_aapDbConnections[Mode::WRITE].size(); i++)
			{
				int CurServer = (WriteServer + i) % (int)m_aapDbConnections[Mode::WRITE].size();
				if(ExecSqlFunc(m_aapDbConnections[Mode::WRITE][i].get(), pThreadData.get(), false))
				{
					WriteServer = CurServer;
					dbg_msg("sql", "%s done on write database %d", pThreadData->m_pName, CurServer);
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
						dbg_msg("sql", "%s done on write backup database %d", pThreadData->m_pName, i);
						Success = true;
						break;
					}
				}
			}
		}
		break;
		}
		if(!Success)
			dbg_msg("sql", "%s failed on all databases", pThreadData->m_pName);
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
	catch(sql::SQLException &e)
	{
		dbg_msg("sql", "%s MySQL Error: %s", pData->m_pName, e.what());
	}
#endif
	catch(std::runtime_error &e)
	{
		dbg_msg("sql", "%s SQLite Error: %s", pData->m_pName, e.what());
	}
	catch(...)
	{
		dbg_msg("sql", "%s Unexpected exception caught", pData->m_pName);
	}
	try
	{
		pConnection->Unlock();
	}
#if defined(CONF_SQL)
	catch(sql::SQLException &e)
	{
		dbg_msg("sql", "%s MySQL Error during unlock: %s", pData->m_pName, e.what());
	}
#endif
	catch(std::runtime_error &e)
	{
		dbg_msg("sql", "%s SQLite Error during unlock: %s", pData->m_pName, e.what());
	}
	catch(...)
	{
		dbg_msg("sql", "%s Unexpected exception caught during unlock", pData->m_pName);
	}
	pConnection->Disconnect();
	if(!Success)
		dbg_msg("sql", "%s failed", pData->m_pName);
	return Success;
}
