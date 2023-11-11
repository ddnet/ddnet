/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "jobs.h"

#include <base/lock_scope.h>

IJob::IJob() :
	m_Status(STATE_PENDING)
{
}

IJob::~IJob() = default;

IJob::EJobState IJob::Status()
{
	return m_Status.load();
}

CJobPool::CJobPool()
{
	// empty the pool
	m_Shutdown = false;
	m_Lock = lock_create();
	sphore_init(&m_Semaphore);
	m_pFirstJob = nullptr;
	m_pLastJob = nullptr;
	m_LockDone = lock_create();
	m_pFirstJobDone = nullptr;
	m_pLastJobDone = nullptr;
}

CJobPool::~CJobPool()
{
	if(!m_Shutdown)
	{
		Destroy();
	}
}

void CJobPool::WorkerThread(void *pUser)
{
	CJobPool *pPool = (CJobPool *)pUser;

	while(!pPool->m_Shutdown)
	{
		// fetch job from queue
		sphore_wait(&pPool->m_Semaphore);
		std::shared_ptr<IJob> pJob = nullptr;
		{
			CLockScope LockScope(pPool->m_Lock);
			if(pPool->m_pFirstJob)
			{
				pJob = pPool->m_pFirstJob;
				pPool->m_pFirstJob = pPool->m_pFirstJob->m_pNext;
				// allow remaining objects in list to destruct, even when current object stays alive
				pJob->m_pNext = nullptr;
				if(!pPool->m_pFirstJob)
					pPool->m_pLastJob = nullptr;
			}
		}

		// run the job if we have one
		if(pJob)
		{
			pJob->m_Status = IJob::STATE_RUNNING;
			pJob->Run();
			pJob->m_Status = IJob::STATE_DONE;

			// add job to queue of done jobs
			{
				CLockScope LockScope(pPool->m_LockDone);
				if(pPool->m_pLastJobDone)
					pPool->m_pLastJobDone->m_pNext = pJob;
				pPool->m_pLastJobDone = std::move(pJob);
				if(!pPool->m_pFirstJobDone)
					pPool->m_pFirstJobDone = pPool->m_pLastJobDone;
			}
		}
	}
}

void CJobPool::Init(int NumThreads)
{
	// start threads
	char aName[32];
	m_vpThreads.reserve(NumThreads);
	for(int i = 0; i < NumThreads; i++)
	{
		str_format(aName, sizeof(aName), "CJobPool worker %d", i);
		m_vpThreads.push_back(thread_init(WorkerThread, this, aName));
	}
}

void CJobPool::Destroy()
{
	m_Shutdown = true;
	for(size_t i = 0; i < m_vpThreads.size(); i++)
		sphore_signal(&m_Semaphore);
	for(void *pThread : m_vpThreads)
		thread_wait(pThread);
	m_vpThreads.clear();
	lock_destroy(m_Lock);
	lock_destroy(m_LockDone);
	sphore_destroy(&m_Semaphore);
}

void CJobPool::Update()
{
	while(true)
	{
		// fetch job from queue of done jobs
		std::shared_ptr<IJob> pJob = nullptr;
		{
			CLockScope LockScope(m_LockDone);
			if(m_pFirstJobDone)
			{
				pJob = m_pFirstJobDone;
				m_pFirstJobDone = m_pFirstJobDone->m_pNext;
				pJob->m_pNext = nullptr;
				if(!m_pFirstJobDone)
					m_pLastJobDone = nullptr;
			}
		}
		if(!pJob)
			break;
		pJob->Done();
	}
}

void CJobPool::Add(std::shared_ptr<IJob> pJob)
{
	// add job to queue
	{
		CLockScope LockScope(m_Lock);
		if(m_pLastJob)
			m_pLastJob->m_pNext = pJob;
		m_pLastJob = std::move(pJob);
		if(!m_pFirstJob)
			m_pFirstJob = m_pLastJob;
	}

	sphore_signal(&m_Semaphore);
}

void CJobPool::RunBlocking(IJob *pJob)
{
	pJob->m_Status = IJob::STATE_RUNNING;
	pJob->Run();
	pJob->m_Status = IJob::STATE_DONE;
	pJob->Done();
}
