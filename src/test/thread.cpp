#include <gtest/gtest.h>

#include <base/system.h>

static void Nothing(void *pUser)
{
	(void)pUser;
}

TEST(Thread, Detach)
{
	void *pThread = thread_init(Nothing, 0);
	thread_detach(pThread);
}

static void SetToOne(void *pUser)
{
	*(int *)pUser = 1;
}

TEST(Thread, Wait)
{
	int Integer = 0;
	void *pThread = thread_init(SetToOne, &Integer);
	thread_wait(pThread);
	EXPECT_EQ(Integer, 1);
}

TEST(Thread, Yield)
{
	thread_yield();
}

TEST(Thread, Semaphore)
{
	SEMAPHORE Semaphore;
	sphore_init(&Semaphore);
	sphore_destroy(&Semaphore);
}

TEST(Thread, SemaphoreSingleThreaded)
{
	SEMAPHORE Semaphore;
	sphore_init(&Semaphore);
	sphore_signal(&Semaphore);
	sphore_signal(&Semaphore);
	sphore_wait(&Semaphore);
	sphore_wait(&Semaphore);
	sphore_destroy(&Semaphore);
}

static void SemaphoreThread(void *pUser)
{
	SEMAPHORE *pSemaphore = (SEMAPHORE *)pUser;
	sphore_wait(pSemaphore);
}

TEST(Thread, SemaphoreMultiThreaded)
{
	SEMAPHORE Semaphore;
	sphore_init(&Semaphore);
	sphore_signal(&Semaphore);
	void *pThread = thread_init(SemaphoreThread, &Semaphore);
	thread_wait(pThread);
	sphore_destroy(&Semaphore);
}

static void LockThread(void *pUser)
{
	LOCK *pLock = (LOCK *)pUser;
	lock_wait(*pLock);
	lock_unlock(*pLock);
}

TEST(Thread, Lock)
{
	LOCK Lock = lock_create();
	void *pThread = thread_init(LockThread, &Lock);
	lock_unlock(Lock);
	thread_wait(pThread);
}
