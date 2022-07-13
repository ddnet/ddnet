/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_JOBS_H
#define ENGINE_SHARED_JOBS_H

#include <base/system.h>

#include <atomic>
#include <memory>

class CJobPool;

class IJob
{
	friend CJobPool;

private:
	std::shared_ptr<IJob> m_pNext = nullptr;

	std::atomic<int> m_Status = 0;
	virtual void Run() = 0;

public:
	IJob();
	IJob(const IJob &Other);
	IJob &operator=(const IJob &Other);
	virtual ~IJob();
	int Status();

	enum
	{
		STATE_PENDING = 0,
		STATE_RUNNING,
		STATE_DONE
	};
};

class CJobPool
{
	enum
	{
		MAX_THREADS = 32
	};
	int m_NumThreads = 0;
	void *m_apThreads[MAX_THREADS] = {nullptr};
	std::atomic<bool> m_Shutdown = false;

	LOCK m_Lock;
	SEMAPHORE m_Semaphore;
	std::shared_ptr<IJob> m_pFirstJob GUARDED_BY(m_Lock) = nullptr;
	std::shared_ptr<IJob> m_pLastJob GUARDED_BY(m_Lock) = nullptr;

	static void WorkerThread(void *pUser) NO_THREAD_SAFETY_ANALYSIS;

public:
	CJobPool();
	~CJobPool();

	void Init(int NumThreads);
	void Destroy();
	void Add(std::shared_ptr<IJob> pJob) REQUIRES(!m_Lock);
	static void RunBlocking(IJob *pJob);
};
#endif
