#include "connection_pool.h"
#include "connection.h"

#include <base/math.h>
#include <base/tl/mpsc.h>
#include <base/tl/threading.h>
#include <engine/console.h>
#include <engine/shared/uuid_manager.h>

#if defined(CONF_SQL)
#include <cppconn/exception.h>
#endif

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <vector>

static int64 millis()
{
	auto duration = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// struct holding data for one sql execution thread
struct CSqlExecData
{
	CSqlExecData(
		FSqlThread pFunc,
		std::unique_ptr<const ISqlData> pThreadData,
		const char *pName,
		bool FaultTolerant)
		: m_pSqlFunc(pFunc),
		  m_pName(pName),
		  m_FaultTolerant(FaultTolerant),
		  m_NumTries(0),
		  m_LastTryTime(0),
		  m_CreateTime(time_timestamp())
	{
		CUuid Id = RandomUuid();
		FormatUuid(Id, m_Id, sizeof(m_Id));
		m_pThreadData = std::move(pThreadData);
	}

	void PrintPerf(const char *pThreadName, const char *pAction)
	{
		dbg_msg("sql_perf", "%s,%s,%s,%s,%lld", m_Id, m_pName, pThreadName, pAction, millis());
	}

	FSqlThread m_pSqlFunc;
	std::unique_ptr<const ISqlData> m_pThreadData;
	const char *m_pName;

	char m_Id[UUID_MAXSTRSIZE];
	bool m_FaultTolerant;
	int m_NumTries;
	int m_LastTryTime;
	int m_CreateTime;
};

// struct holding one task to process
struct CSqlTask
{
	enum Task
	{
		NONE,
		ADD_SQL_SERVER,
		SET_SQL_BACKUP_SERVER,
		EXECUTE_THREAD,
		PRINT_SQL_SERVER,
		SHUTDOWN,
	} m_Task;

	// no using a union, since unions don't support non-POD (plain old data)
	// set if m_Task is either ADD_SQL_SERVER or ADD_SQL_BACKUP_SERVER
	std::unique_ptr<IDbConnection> m_Db;
	// set if m_Task is EXECUTE_THREAD
	std::unique_ptr<struct CSqlExecData> m_ExecData;

	static CSqlTask ExecuteThread(std::unique_ptr<struct CSqlExecData> pThreadData)
	{
		return CSqlTask(EXECUTE_THREAD, nullptr, std::move(pThreadData));
	}
	static CSqlTask RegisterDb(std::unique_ptr<IDbConnection> pDb)
	{
		return CSqlTask(ADD_SQL_SERVER, std::move(pDb), nullptr);
	}
	static CSqlTask SetBackupDb(std::unique_ptr<IDbConnection> pDb)
	{
		return CSqlTask(SET_SQL_BACKUP_SERVER, std::move(pDb), nullptr);
	}
	static CSqlTask Print()
	{
		return CSqlTask(PRINT_SQL_SERVER, nullptr, nullptr);
	}
	static CSqlTask Shutdown()
	{
		return CSqlTask(SHUTDOWN, nullptr, nullptr);
	}

	CSqlTask()
		: m_Task(NONE)
	{
	}

private:
	CSqlTask(Task t, std::unique_ptr<IDbConnection> pDb, std::unique_ptr<struct CSqlExecData> pThreadData)
		: m_Task(t)
	{
		m_Db = std::move(pDb);
		m_ExecData = std::move(pThreadData);
	}
};

struct CWorker
{
	// Mode in which the thread using this queue operates
	CDbConnectionPool::Mode m_Mode;
	CMpsc<CSqlTask, 512> m_Queue;

	CWorker(CDbConnectionPool::Mode Mode)
		: m_Mode(Mode)
	{
	}
};

// private implementation of the connection pool
struct CDbConnectionPool::Impl
{
	Impl();
	std::vector<std::unique_ptr<IDbConnection>> m_aapDbConnections[CDbConnectionPool::NUM_MODES];

	static void ReadWorker(void *pUser);
	static void WriteWorker(void *pUser);
	static void WriteBackupWorker(void *pUser);

	void Worker(CWorker *pWorker);
	bool ExecSqlFunc(IDbConnection *pConnection, struct CSqlExecData *pData, bool Failure);

	// three queues for inter-thread communication:
	// 1. main thread -> read thread
	// 2. main thread -> write thread
	// 3. main thread, write thread, write_backup thread -> write_backup thread
	CWorker m_aWorker[CDbConnectionPool::NUM_MODES];

	// Threads can relay information back to the main thread by putting information into
	// this queue
	CMpsc<CSqlResponse, 64> m_Responses;

	// stores the number of threads, that didn't shutdown yet, after OnShutdown was issued
	std::atomic_int m_Shutdown;
};

CDbConnectionPool::Impl::Impl()
	: m_aWorker{CWorker(READ), CWorker(WRITE), CWorker(WRITE_BACKUP)}
{
	thread_init_and_detach(Impl::ReadWorker, this, "database read worker thread");
	thread_init_and_detach(Impl::WriteWorker, this, "database write worker thread");
	thread_init_and_detach(Impl::WriteBackupWorker, this, "database write backup worker thread");
}

CDbConnectionPool::CDbConnectionPool()
{
	pImpl = std::unique_ptr<Impl>(new Impl());
}

CDbConnectionPool::~CDbConnectionPool()
{
}

// public interface of the connection pool
void CDbConnectionPool::Print(IConsole *pConsole, Mode DatabaseMode)
{
	if(0 <= DatabaseMode && DatabaseMode < Mode::NUM_MODES)
		pImpl->m_aWorker[DatabaseMode].m_Queue.Send(std::move(CSqlTask::Print()));
}

void CDbConnectionPool::RegisterDatabase(std::unique_ptr<IDbConnection> pDatabase, Mode DatabaseMode)
{
	switch(DatabaseMode)
	{
	case READ:
		pImpl->m_aWorker[READ].m_Queue.Send(std::move(CSqlTask::RegisterDb(std::move(pDatabase))));
		break;
	case WRITE:
		pImpl->m_aWorker[WRITE].m_Queue.Send(std::move(CSqlTask::RegisterDb(
			std::move(std::unique_ptr<IDbConnection>(pDatabase->Copy())))));
		pImpl->m_aWorker[WRITE_BACKUP].m_Queue.Send(std::move(CSqlTask::RegisterDb(std::move(pDatabase))));
		break;
	case WRITE_BACKUP:
		pImpl->m_aWorker[WRITE_BACKUP].m_Queue.Send(std::move(CSqlTask::SetBackupDb(std::move(pDatabase))));
		break;
	case NUM_MODES:
		break;
	}
}

void CDbConnectionPool::ExecuteRead(
	FSqlThread pFunc,
	std::unique_ptr<const ISqlData> pThreadData,
	const char *pName)
{
	auto pTask = std::unique_ptr<CSqlExecData>(new CSqlExecData(pFunc, std::move(pThreadData), pName, false));
	pTask->PrintPerf("Main", "Create");
	pImpl->m_aWorker[READ].m_Queue.Send(std::move(CSqlTask::ExecuteThread(std::move(pTask))));
}

void CDbConnectionPool::ExecuteWrite(
	FSqlThread pFunc,
	std::unique_ptr<const ISqlData> pThreadData,
	const char *pName)
{
	auto pTask = std::unique_ptr<CSqlExecData>(new CSqlExecData(pFunc, std::move(pThreadData), pName, false));
	pTask->PrintPerf("Main", "Create");
	pImpl->m_aWorker[WRITE].m_Queue.Send(std::move(CSqlTask::ExecuteThread(std::move(pTask))));
}

void CDbConnectionPool::ExecuteWriteFaultTolerant(
	FSqlThread pFunc,
	std::unique_ptr<const ISqlData> pThreadData,
	const char *pName)
{
	auto pTask = std::unique_ptr<CSqlExecData>(new CSqlExecData(pFunc, std::move(pThreadData), pName, true));
	pTask->PrintPerf("Main", "Create");
	pImpl->m_aWorker[WRITE].m_Queue.Send(std::move(CSqlTask::ExecuteThread(std::move(pTask))));
}

bool CDbConnectionPool::GetResponse(CSqlResponse *pResponse)
{
	return pImpl->m_Responses.TryRecv(pResponse);
}

void CDbConnectionPool::OnShutdown()
{
	pImpl->m_Shutdown.store(NUM_MODES);
	// "close" the sender from the main thread
	for(int i = 0; i < NUM_MODES; i++)
		pImpl->m_aWorker[i].m_Queue.Send(std::move(CSqlTask::Shutdown()));
	int i = 0;
	while(pImpl->m_Shutdown.load() != 0)
	{
		if (i > 600)  {
			dbg_msg("sql", "Waited 60 seconds for score-threads to complete, quitting anyway");
			break;
		}

		// print a log about every two seconds
		if (i % 20 == 0)
			dbg_msg("sql", "Waiting for score-threads to complete (%ds)", i / 10);
		++i;
		thread_sleep(100000);
	}
}

void CDbConnectionPool::Impl::ReadWorker(void *pUser)
{
	CDbConnectionPool::Impl *pThis = (CDbConnectionPool::Impl *)pUser;
	pThis->Worker(&pThis->m_aWorker[READ]);
}

void CDbConnectionPool::Impl::WriteWorker(void *pUser)
{
	CDbConnectionPool::Impl *pThis = (CDbConnectionPool::Impl *)pUser;
	pThis->Worker(&pThis->m_aWorker[WRITE]);
}

void CDbConnectionPool::Impl::WriteBackupWorker(void *pUser)
{
	CDbConnectionPool::Impl *pThis = (CDbConnectionPool::Impl *)pUser;
	pThis->Worker(&pThis->m_aWorker[WRITE_BACKUP]);
}

void CDbConnectionPool::Impl::Worker(CWorker *pWorker)
{
	// remember last working server and try to connect to it first
	int LastServer = 0;
	std::vector<std::unique_ptr<IDbConnection>> apDb;
	std::unique_ptr<IDbConnection> pDbBackup = nullptr;
	const char *ModeDesc[] = {"Read", "Write", "WriteBackup"};
	int NumClosed = 0;
	while(1)
	{
		CSqlTask Task = pWorker->m_Queue.Recv();
		switch(Task.m_Task)
		{
		case CSqlTask::NONE:
			dbg_msg("sql_pool", "received empty job, which shouldn't happen");
			break;
		case CSqlTask::ADD_SQL_SERVER:
		{
			if(Task.m_Db == nullptr)
				dbg_msg("sql_pool", "received invalid add_sql_server request (nullptr)");
			else
				apDb.push_back(std::move(Task.m_Db));
			break;
		}
		case CSqlTask::SET_SQL_BACKUP_SERVER:
		{
			if(Task.m_Db == nullptr)
				dbg_msg("sql_pool", "received invalid set_sql_backup_server request (nullptr)");
			else
				pDbBackup = std::move(Task.m_Db);
			break;
		}
		case CSqlTask::EXECUTE_THREAD:
		{
			Task.m_ExecData->m_NumTries += 1;
			Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], "Dequeue");
			if(pWorker->m_Mode == WRITE_BACKUP && !m_Shutdown)
			{
				// delay retries around 10 sec (convert seconds to microseconds) to prevent flooding the sql server
				// but also process queries in time
				thread_sleep(clamp(10 + time_timestamp() - Task.m_ExecData->m_LastTryTime, 0, 10) * 1000000);
				Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], "Wait");
			}
			bool Success = false;
			for(int i = 0; i < (int)apDb.size(); i++)
			{
				if(m_Shutdown)
				{
					// When shutting down, always use backup SQLite DB to speed things up, discarding reads
					break;
				}
				int CurServer = (LastServer + i) % (int)apDb.size();
				if(ExecSqlFunc(apDb[CurServer].get(), Task.m_ExecData.get(), false))
				{
					char aBuf[16];
					str_format(aBuf, sizeof(aBuf), "Success %d", CurServer);
					Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], aBuf);
					LastServer = CurServer;
					dbg_msg("sql", "%s done on %s database %d", Task.m_ExecData->m_pName, ModeDesc[pWorker->m_Mode], CurServer);
					Success = true;
					break;
				}
				else
				{
					char aBuf[16];
					str_format(aBuf, sizeof(aBuf), "Failure %d", CurServer);
					Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], aBuf);
				}
			}
			Task.m_ExecData->m_LastTryTime = time_timestamp();
			if(!Success && pWorker->m_Mode == WRITE)
			{
				Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], "EnqueueBackup");
				m_aWorker[WRITE_BACKUP].m_Queue.Send(std::move(Task));
			}
			else if(!Success && pWorker->m_Mode == WRITE_BACKUP)
			{
				if(Task.m_ExecData->m_FaultTolerant && !m_Shutdown && (Task.m_ExecData->m_NumTries < 3 || time_timestamp() < Task.m_ExecData->m_CreateTime + 60))
				{
					// retry FaultTolerant write requests
					Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], "EnqueueBackup");
					m_aWorker[WRITE_BACKUP].m_Queue.Send(std::move(Task));
				}
				else
				{
					// retried some times already or shutting down, last chance is the backup SQLite server
					if(ExecSqlFunc(pDbBackup.get(), Task.m_ExecData.get(), true))
					{
						Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], "Success Backup");
						dbg_msg("sql", "%s done on write backup database", Task.m_ExecData->m_pName);
						Success = true;
						break;
					}
					else
					{
						Task.m_ExecData->PrintPerf(ModeDesc[pWorker->m_Mode], "Failure Backup");
						dbg_msg("sql", "%s failed on all databases", Task.m_ExecData->m_pName);
					}
				}
			}
			else if(!Success && pWorker->m_Mode == READ)
			{
				// failed reads are ok, just log it
				dbg_msg("sql", "%s failed on all databases", Task.m_ExecData->m_pName);
			}
			break;
		}
		case CSqlTask::PRINT_SQL_SERVER:
		{
			char aBuf[512];
			for(unsigned int i = 0; i < apDb.size(); i++)
			{
				apDb[i]->Format(aBuf, sizeof(aBuf));
				CSqlResponse Response = CSqlResponse(CSqlResponse::CONSOLE);
				str_format(Response.aMsg, sizeof(Response.aMsg), "%s: %s",
					ModeDesc[pWorker->m_Mode], aBuf);
				m_Responses.Send(Response);
			}
			if(pDbBackup != nullptr)
			{
				pDbBackup->Format(aBuf, sizeof(aBuf));
				CSqlResponse Response = CSqlResponse(CSqlResponse::CONSOLE);
				str_format(Response.aMsg, sizeof(Response.aMsg), "%s (Fallback): %s",
					ModeDesc[pWorker->m_Mode], aBuf);
				m_Responses.Send(Response);
			}
			break;
		}
		case CSqlTask::SHUTDOWN:
			NumClosed += 1;
			// the WRITE thread is responsible to notify WRITE_BACKUP that no further
			// queries get send from the WRITE thread
			if(pWorker->m_Mode == CDbConnectionPool::WRITE)
			{
				m_aWorker[WRITE_BACKUP].m_Queue.Send(std::move(Task));
			}
			if(pWorker->m_Mode == CDbConnectionPool::WRITE_BACKUP)
			{
				// main thread and write thread closed their Sender to this queue,
				// now we are the last thread sending messages to this queue;
				// close our end at last
				if(NumClosed == 2)
					m_aWorker[WRITE_BACKUP].m_Queue.Send(std::move(Task));
				// do not exit backup thread yet until we received all three closing notifications (including ours)
				if(NumClosed < 3)
					break;
			}
			// signal, that all jobs got processed and thread exits
			m_Shutdown.fetch_sub(1);
			return;
		}
		/*
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
		} break;
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
		} break;
		}
		if(!Success)
			dbg_msg("sql", "%s failed on all databases", pThreadData->m_pName);
	*/
	}
}

bool CDbConnectionPool::Impl::ExecSqlFunc(IDbConnection *pConnection, CSqlExecData *pData, bool Failure)
{
	if(pConnection->Connect() != IDbConnection::SUCCESS)
		return false;
	bool Success = false;
	try
	{
		if(pData->m_pSqlFunc(pConnection, pData->m_pThreadData.get(), Failure))
			Success = true;
	}
#if defined(CONF_SQL)
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "%s MySQL Error: %s", pData->m_pName, e.what());
	}
#endif
	catch (std::runtime_error &e)
	{
		dbg_msg("sql", "%s SQLite Error: %s", pData->m_pName, e.what());
	}
	catch (...)
	{
		dbg_msg("sql", "%s Unexpected exception caught", pData->m_pName);
	}
	try
	{
		pConnection->Unlock();
	}
#if defined(CONF_SQL)
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "%s MySQL Error during unlock: %s", pData->m_pName, e.what());
	}
#endif
	catch (std::runtime_error &e)
	{
		dbg_msg("sql", "%s SQLite Error during unlock: %s", pData->m_pName, e.what());
	}
	catch (...)
	{
		dbg_msg("sql", "%s Unexpected exception caught during unlock", pData->m_pName);
	}
	pConnection->Disconnect();
	if(!Success)
		dbg_msg("sql", "%s failed", pData->m_pName);
	return Success;
}
