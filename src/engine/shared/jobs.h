/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_JOBS_H
#define ENGINE_SHARED_JOBS_H

#include <base/system.h>
#include <engine/engine.h>

#include <atomic>
#include <memory>

class CJobPool;
class CEngine;

class IJob : public IEngineRunnable
{
	friend CJobPool;
	friend CEngine;
	inline static int m_sRunner;

private:
	std::shared_ptr<IJob> m_pNext;

	virtual void Run() override = 0;

public:
	virtual ~IJob();

	virtual int Runner() final { return m_sRunner; };
};

class CJobPool : public IEngineRunner
{
	enum
	{
		MAX_THREADS = 32
	};
	int m_NumThreads;
	void *m_apThreads[MAX_THREADS];
	std::atomic<bool> m_Shutdown;

	LOCK m_Lock;
	SEMAPHORE m_Semaphore;
	std::shared_ptr<IJob> m_pFirstJob GUARDED_BY(m_Lock);
	std::shared_ptr<IJob> m_pLastJob GUARDED_BY(m_Lock);

	static void WorkerThread(void *pUser) NO_THREAD_SAFETY_ANALYSIS;

public:
	CJobPool();
	~CJobPool();

	void Init(int NumThreads);
	virtual void Shutdown() override;
	void Add(std::shared_ptr<IJob> pJob);
	virtual void Run(std::shared_ptr<IEngineRunnable> pRunnable) override;
	static void RunBlocking(IJob *pJob);
};
#endif
