/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include "jobs.h"

IJob::IJob() :
	m_Status(STATE_PENDING)
{
}

IJob::IJob(const IJob &Other) :
	m_Status(STATE_PENDING)
{
}

IJob &IJob::operator=(const IJob &Other)
{
	m_Status = STATE_PENDING;
	return *this;
}

IJob::~IJob()
{
}

int IJob::Status()
{
	return m_Status.load();
}

CJobPool::CJobPool()
{
	// empty the pool
	m_NumThreads = 0;
	m_Shutdown = false;
	m_Lock = lock_create();
	sphore_init(&m_Semaphore);
	m_pFirstJob = 0;
	m_pLastJob = 0;
}

CJobPool::~CJobPool()
{
	m_Shutdown = true;
	for(int i = 0; i < m_NumThreads; i++)
		sphore_signal(&m_Semaphore);
	for(int i = 0; i < m_NumThreads; i++)
		thread_wait(m_apThreads[i]);
	lock_destroy(m_Lock);
	sphore_destroy(&m_Semaphore);
}

void CJobPool::WorkerThread(void *pUser)
{
	CJobPool *pPool = (CJobPool *)pUser;

	while(!pPool->m_Shutdown)
	{
		std::shared_ptr<IJob> pJob = 0;

		// fetch job from queue
		sphore_wait(&pPool->m_Semaphore);
		lock_wait(pPool->m_Lock);
		if(pPool->m_pFirstJob)
		{
			pJob = pPool->m_pFirstJob;
			pPool->m_pFirstJob = pPool->m_pFirstJob->m_pNext;
			if(!pPool->m_pFirstJob)
				pPool->m_pLastJob = 0;
		}
		lock_unlock(pPool->m_Lock);

		// do the job if we have one
		if(pJob)
		{
			pJob->m_Status = IJob::STATE_RUNNING;
			pJob->Run();
			pJob->m_Status = IJob::STATE_DONE;
		}
	}

}

void CJobPool::Init(int NumThreads)
{
	// start threads
	m_NumThreads = NumThreads > MAX_THREADS ? MAX_THREADS : NumThreads;
	for(int i = 0; i < NumThreads; i++)
		m_apThreads[i] = thread_init(WorkerThread, this);
}

void CJobPool::Add(std::shared_ptr<IJob> pJob)
{
	lock_wait(m_Lock);

	// add job to queue
	if(m_pLastJob)
		m_pLastJob->m_pNext = pJob;
	m_pLastJob = std::move(pJob);
	if(!m_pFirstJob)
		m_pFirstJob = m_pLastJob;

	lock_unlock(m_Lock);
	sphore_signal(&m_Semaphore);
}
