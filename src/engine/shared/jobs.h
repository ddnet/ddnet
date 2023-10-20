/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_JOBS_H
#define ENGINE_SHARED_JOBS_H

#include <base/system.h>

#include <atomic>
#include <memory>
#include <vector>

class CJobPool;

class IJob
{
	friend CJobPool;

public:
	enum EJobState
	{
		STATE_PENDING = 0,
		STATE_RUNNING,
		STATE_DONE
	};

private:
	std::shared_ptr<IJob> m_pNext;
	std::atomic<EJobState> m_Status;

	/**
	 * Performs tasks in a worker thread.
	 */
	virtual void Run() = 0;

	/**
	 * Performs final tasks on the main thread after job is done.
	 */
	virtual void Done()
	{
	}

public:
	IJob();
	IJob(const IJob &Other) = delete;
	IJob &operator=(const IJob &Other) = delete;
	virtual ~IJob();
	EJobState Status();
};

class CJobPool
{
	std::vector<void *> m_vpThreads;
	std::atomic<bool> m_Shutdown;

	LOCK m_Lock;
	SEMAPHORE m_Semaphore;
	std::shared_ptr<IJob> m_pFirstJob GUARDED_BY(m_Lock);
	std::shared_ptr<IJob> m_pLastJob GUARDED_BY(m_Lock);

	LOCK m_LockDone;
	std::shared_ptr<IJob> m_pFirstJobDone GUARDED_BY(m_LockDone);
	std::shared_ptr<IJob> m_pLastJobDone GUARDED_BY(m_LockDone);

	static void WorkerThread(void *pUser) NO_THREAD_SAFETY_ANALYSIS;

public:
	CJobPool();
	~CJobPool();

	/**
	 * Initializes the job pool with the given number of worker threads.
	 *
	 * @param NumTheads The number of worker threads.
	 *
	 * @remark Must be called on the main thread.
	 */
	void Init(int NumThreads);

	/**
	 * Destroys the job pool. Waits for worker threads to complete their current jobs.
	 *
	 * @remark Must be called on the main thread.
	 */
	void Destroy();

	/**
	 * Updates the job pool, calling the `Done` function for completed jobs.
	 *
	 * @remark Must be called on the main thread.
	 */
	void Update() REQUIRES(!m_LockDone);

	/**
	 * Adds a job to the queue of the job pool.
	 *
	 * @param pJob The job to enqueue.
	 */
	void Add(std::shared_ptr<IJob> pJob) REQUIRES(!m_Lock);

	/**
	 * Runs a job's lifecycle functions in the current thread.
	 *
	 * @param pJob The job to run.
	 *
	 * @remark Must be called on the main thread, if the job has
	 * a `Done` function must be called on the main thread.
	 */
	static void RunBlocking(IJob *pJob);
};
#endif
