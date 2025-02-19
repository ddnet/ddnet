#include <gtest/gtest.h>

#include <base/lock.h>
#include <base/system.h>
#include <base/tl/threading.h>

static void Nothing(void *pUser)
{
	(void)pUser;
}

TEST(Thread, Detach)
{
	void *pThread = thread_init(Nothing, nullptr, "detach");
	thread_detach(pThread);
}

static void SetToOne(void *pUser)
{
	*(int *)pUser = 1;
}

TEST(Thread, Wait)
{
	int Integer = 0;
	void *pThread = thread_init(SetToOne, &Integer, "wait");
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

TEST(Thread, SemaphoreWrapperSingleThreaded)
{
	CSemaphore Semaphore;
	EXPECT_EQ(Semaphore.GetApproximateValue(), 0);
	Semaphore.Signal();
	EXPECT_EQ(Semaphore.GetApproximateValue(), 1);
	Semaphore.Signal();
	EXPECT_EQ(Semaphore.GetApproximateValue(), 2);
	Semaphore.Wait();
	EXPECT_EQ(Semaphore.GetApproximateValue(), 1);
	Semaphore.Wait();
	EXPECT_EQ(Semaphore.GetApproximateValue(), 0);
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
	void *pThread = thread_init(SemaphoreThread, &Semaphore, "semaphore");
	thread_wait(pThread);
	sphore_destroy(&Semaphore);
}

static void LockThread(void *pUser)
{
	CLock *pLock = (CLock *)pUser;
	pLock->lock();
	pLock->unlock();
}

TEST(Thread, Lock)
{
	CLock Lock;
	Lock.lock();
	void *pThread = thread_init(LockThread, &Lock, "lock");
	Lock.unlock();
	thread_wait(pThread);
}
