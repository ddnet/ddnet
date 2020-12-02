#ifndef BASE_TL_THREADING_H
#define BASE_TL_THREADING_H

#include "../system.h"

class CSemaphore
{
	SEMAPHORE m_Sem;

public:
	CSemaphore() { sphore_init(&m_Sem); }
	~CSemaphore() { sphore_destroy(&m_Sem); }
	CSemaphore(const CSemaphore &) = delete;
	void Wait() { sphore_wait(&m_Sem); }
	void Signal() { sphore_signal(&m_Sem); }
};

class SCOPED_CAPABILITY CLock
{
	LOCK m_Lock;

public:
	CLock() ACQUIRE(m_Lock)
	{
		m_Lock = lock_create();
	}

	~CLock() RELEASE()
	{
		lock_destroy(m_Lock);
	}

	CLock(const CLock &) = delete;

	void Take() ACQUIRE(m_Lock) { lock_wait(m_Lock); }
	void Release() RELEASE() { lock_unlock(m_Lock); }
};

class CScopeLock
{
	CLock *m_pLock;

public:
	CScopeLock(CLock *pLock)
	{
		m_pLock = pLock;
		m_pLock->Take();
	}

	~CScopeLock()
	{
		m_pLock->Release();
	}

	CScopeLock(const CScopeLock &) = delete;
};

#endif // BASE_TL_THREADING_H
