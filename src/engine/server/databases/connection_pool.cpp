#include "connection_pool.h"
#include "connection.h"

#include <base/system.h>

#include <chrono>
#include <iterator>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

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
	~CSqlExecData() = default;

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

	m_FirstElem(0),
	m_LastElem(0)
{
	thread_init_and_detach(CDbConnectionPool::Worker, this, "database worker thread");
}

CDbConnectionPool::~CDbConnectionPool() = default;

void CDbConnectionPool::Print(IConsole *pConsole, Mode DatabaseMode)
{
	static const char *s_apModeDesc[] = {"Read", "Write", "WriteBackup"};
	for(unsigned int i = 0; i < m_vvpDbConnections[DatabaseMode].size(); i++)
	{
		m_vvpDbConnections[DatabaseMode][i]->Print(pConsole, s_apModeDesc[DatabaseMode]);
	}
}

void CDbConnectionPool::RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode)
{
	if(DatabaseMode < 0 || NUM_MODES <= DatabaseMode)
		return;
	m_vvpDbConnections[DatabaseMode].push_back(std::move(pDatabase));
}

void CDbConnectionPool::Execute(
	FRead pFunc,
	std::unique_ptr<const ISqlData> pSqlRequestData,
	const char *pName)
{
	m_aTasks[m_FirstElem++] = std::make_unique<CSqlExecData>(pFunc, std::move(pSqlRequestData), pName);
	m_FirstElem %= std::size(m_aTasks);
	m_NumElem.Signal();
}

void CDbConnectionPool::ExecuteWrite(
	FWrite pFunc,
	std::unique_ptr<const ISqlData> pSqlRequestData,
	const char *pName)
{
	m_aTasks[m_FirstElem++] = std::make_unique<CSqlExecData>(pFunc, std::move(pSqlRequestData), pName);
	m_FirstElem %= std::size(m_aTasks);
	m_NumElem.Signal();
}

void CDbConnectionPool::OnShutdown()
{
	m_Shutdown.store(true);
	m_NumElem.Signal();
	int i = 0;
	while(m_Shutdown.load())
	{
		// print a log about every two seconds
		if(i % 20 == 0 && i > 0)
		{
			dbg_msg("sql", "Waiting for score threads to complete (%ds)", i / 10);
		}
		++i;
		std::this_thread::sleep_for(100ms);
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
	// enter fail mode when a sql request fails, skip read request during it and
	// write to the backup database until all requests are handled
	bool FailMode = false;
	while(true)
	{
		if(FailMode && m_NumElem.GetApproximateValue() == 0)
		{
			FailMode = false;
		}
		m_NumElem.Wait();
		auto pThreadData = std::move(m_aTasks[m_LastElem++]);
		// work through all database jobs after OnShutdown is called before exiting the thread
		if(pThreadData == nullptr)
		{
			m_Shutdown.store(false);
			return;
		}
		m_LastElem %= std::size(m_aTasks);
		bool Success = false;
		switch(pThreadData->m_Mode)
		{
		case CSqlExecData::READ_ACCESS:
		{
			for(int i = 0; i < (int)m_vvpDbConnections[Mode::READ].size(); i++)
			{
				if(m_Shutdown)
				{
					dbg_msg("sql", "%s dismissed read request during shutdown", pThreadData->m_pName);
					break;
				}
				if(FailMode)
				{
					dbg_msg("sql", "%s dismissed read request during FailMode", pThreadData->m_pName);
					break;
				}
				int CurServer = (ReadServer + i) % (int)m_vvpDbConnections[Mode::READ].size();
				if(ExecSqlFunc(m_vvpDbConnections[Mode::READ][CurServer].get(), pThreadData.get(), false))
				{
					ReadServer = CurServer;
					dbg_msg("sql", "%s done on read database %d", pThreadData->m_pName, CurServer);
					Success = true;
					break;
				}
			}
			if(!Success)
			{
				FailMode = true;
			}
		}
		break;
		case CSqlExecData::WRITE_ACCESS:
		{
			for(int i = 0; i < (int)m_vvpDbConnections[Mode::WRITE].size(); i++)
			{
				if(m_Shutdown && !m_vvpDbConnections[Mode::WRITE_BACKUP].empty())
				{
					dbg_msg("sql", "%s skipped to backup database during shutdown", pThreadData->m_pName);
					break;
				}
				if(FailMode && !m_vvpDbConnections[Mode::WRITE_BACKUP].empty())
				{
					dbg_msg("sql", "%s skipped to backup database during FailMode", pThreadData->m_pName);
					break;
				}
				int CurServer = (WriteServer + i) % (int)m_vvpDbConnections[Mode::WRITE].size();
				if(ExecSqlFunc(m_vvpDbConnections[Mode::WRITE][i].get(), pThreadData.get(), false))
				{
					WriteServer = CurServer;
					dbg_msg("sql", "%s done on write database %d", pThreadData->m_pName, CurServer);
					Success = true;
					break;
				}
			}
			if(!Success)
			{
				FailMode = true;
				for(int i = 0; i < (int)m_vvpDbConnections[Mode::WRITE_BACKUP].size(); i++)
				{
					if(ExecSqlFunc(m_vvpDbConnections[Mode::WRITE_BACKUP][i].get(), pThreadData.get(), true))
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
		if(pThreadData->m_pThreadData->m_pResult != nullptr)
		{
			pThreadData->m_pThreadData->m_pResult->m_Success = Success;
			pThreadData->m_pThreadData->m_pResult->m_Completed.store(true);
		}
	}
}

bool CDbConnectionPool::ExecSqlFunc(IDbConnection *pConnection, CSqlExecData *pData, bool Failure)
{
	char aError[256] = "error message not initialized";
	if(pConnection->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return false;
	}
	bool Success = false;
	switch(pData->m_Mode)
	{
	case CSqlExecData::READ_ACCESS:
		Success = !pData->m_Ptr.m_pReadFunc(pConnection, pData->m_pThreadData.get(), aError, sizeof(aError));
		break;
	case CSqlExecData::WRITE_ACCESS:
		Success = !pData->m_Ptr.m_pWriteFunc(pConnection, pData->m_pThreadData.get(), Failure, aError, sizeof(aError));
		break;
	}
	pConnection->Disconnect();
	if(!Success)
	{
		dbg_msg("sql", "%s failed: %s", pData->m_pName, aError);
	}
	return Success;
}
