
#pragma once

#include <mutex>
#include <chrono>
#include <utility>
#include <type_traits>
#include <condition_variable>

#include "../system.h"

/*
	atomic_inc - should return the value after increment
	atomic_dec - should return the value after decrement
	atomic_compswap - should return the value before the eventual swap
	sync_barrier - creates a full hardware fence
*/

#if defined(__GNUC__)

	inline unsigned atomic_inc(volatile unsigned *pValue)
	{
		return __sync_add_and_fetch(pValue, 1);
	}

	inline unsigned atomic_dec(volatile unsigned *pValue)
	{
		return __sync_add_and_fetch(pValue, -1);
	}

	inline unsigned atomic_compswap(volatile unsigned *pValue, unsigned comperand, unsigned value)
	{
		return __sync_val_compare_and_swap(pValue, comperand, value);
	}

	inline void sync_barrier()
	{
		__sync_synchronize();
	}

#elif defined(_MSC_VER)
	#include <intrin.h>

	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>

	inline unsigned atomic_inc(volatile unsigned *pValue)
	{
		return _InterlockedIncrement((volatile long *)pValue);
	}

	inline unsigned atomic_dec(volatile unsigned *pValue)
	{
		return _InterlockedDecrement((volatile long *)pValue);
	}

	inline unsigned atomic_compswap(volatile unsigned *pValue, unsigned comperand, unsigned value)
	{
		return _InterlockedCompareExchange((volatile long *)pValue, (long)value, (long)comperand);
	}

	inline void sync_barrier()
	{
		MemoryBarrier();
	}
#else
	#error missing atomic implementation for this compiler
#endif

class semaphore
{
	SEMAPHORE sem;
public:
	semaphore() { sphore_init(&sem); }
	~semaphore() { sphore_destroy(&sem); }
	void wait() { sphore_wait(&sem); }
	void signal() { sphore_signal(&sem); }
};

class lock
{
	friend class scope_lock;

	LOCK var;

	void take() { lock_wait(var); }
	void release() { lock_unlock(var); }

public:
	lock()
	{
		var = lock_create();
	}

	~lock()
	{
		lock_destroy(var);
	}
};

class scope_lock
{
	lock *var;
public:
	scope_lock(lock *l)
	{
		var = l;
		var->take();
	}

	~scope_lock()
	{
		var->release();
	}
};

template <
	typename M = std::mutex,
	typename CV = std::conditional<
		std::is_same<M, std::mutex>::value,
		std::condition_variable,
		std::condition_variable_any>>
class condition_variable
{
public:
	using CVStatus = std::cv_status;

	condition_variable() = default;
	condition_variable(const condition_variable &) = delete;
	condition_variable &operator=(const condition_variable &) = delete;

	template <typename LO>
	std::unique_lock<M> GetLock(LO LockOpt)
	{
		return std::unique_lock<M>(m_Mutex, LockOpt);
	}

	std::unique_lock<M> GetLock() { return std::unique_lock<M>(m_Mutex); }

	void NotifyOne() { m_CV.notify_one(); }

	void NotifyAll() { m_CV.notify_all(); }

	void Wait(std::unique_lock<M> &Lck) { m_CV.wait(Lck); }

	template <typename Predicate>
	void Wait(std::unique_lock<M> &Lck, Predicate &&Pred)
	{
		m_CV.wait(Lck, std::forward<Predicate>(Pred));
	}

	CVStatus WaitFor(std::unique_lock<M> &Lck, int MSecs)
	{
		return m_CV.wait_for(Lck, std::chrono::milliseconds(MSecs));
	}

	template <typename Predicate>
	bool WaitFor(std::unique_lock<M> &Lck, int MSecs, Predicate &&Pred)
	{
		return m_CV.wait_for(
			Lck,
			std::chrono::milliseconds(MSecs),
			std::forward<Predicate>(Pred));
	}

	M &Mutex() { return m_Mutex; }

private:
	M m_Mutex;
	std::condition_variable_any m_CV;
};
