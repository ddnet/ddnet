#ifndef BASE_TL_THREADING_H
#define BASE_TL_THREADING_H

#include "../system.h"
#include <atomic>

class CSemaphore
{
	SEMAPHORE m_Sem;
	// implement the counter separately, because the `sem_getvalue`-API is
	// deprecated on macOS: https://stackoverflow.com/a/16655541
	std::atomic_int m_Count{0};

public:
	CSemaphore() { sphore_init(&m_Sem); }
	~CSemaphore() { sphore_destroy(&m_Sem); }
	CSemaphore(const CSemaphore &) = delete;
	int GetApproximateValue() { return m_Count.load(); }
	void Wait()
	{
		sphore_wait(&m_Sem);
		m_Count.fetch_sub(1);
	}
	void Signal()
	{
		m_Count.fetch_add(1);
		sphore_signal(&m_Sem);
	}
};

#endif // BASE_TL_THREADING_H
