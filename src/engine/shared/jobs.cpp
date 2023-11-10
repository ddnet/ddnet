/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "jobs.h"

IJob::IJob() :
	m_Status(STATE_PENDING)
{
}

IJob::~IJob() = default;

int IJob::Status()
{
	return m_Status.load();
}

CJobPool::CJobPool()
{
	// empty the pool
	m_Shutdown = false;
	sphore_init(&m_Semaphore);
	m_pFirstJob = 0;
	m_pLastJob = 0;
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
		std::shared_ptr<IJob> pJob = 0;

		// fetch job from queue
		sphore_wait(&pPool->m_Semaphore);
		{
			CLockScope ls(pPool->m_Lock);
			if(pPool->m_pFirstJob)
			{
				pJob = pPool->m_pFirstJob;
				pPool->m_pFirstJob = pPool->m_pFirstJob->m_pNext;
				// allow remaining objects in list to destruct, even when current object stays alive
				pJob->m_pNext = nullptr;
				if(!pPool->m_pFirstJob)
					pPool->m_pLastJob = 0;
			}
		}

		// do the job if we have one
		if(pJob)
		{
			RunBlocking(pJob.get());
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
	sphore_destroy(&m_Semaphore);
}

void CJobPool::Add(std::shared_ptr<IJob> pJob)
{
	{
		CLockScope ls(m_Lock);
		// add job to queue
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
}
