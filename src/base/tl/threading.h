#ifndef BASE_TL_THREADING_H
#define BASE_TL_THREADING_H

#include "../system.h"

class semaphore
{
	SEMAPHORE sem;

public:
	semaphore() { sphore_init(&sem); }
	~semaphore() { sphore_destroy(&sem); }
	semaphore(const semaphore &) = delete;
	void wait() { sphore_wait(&sem); }
	void signal() { sphore_signal(&sem); }
};

class lock
{
	LOCK var;

public:
	lock()
	{
		var = lock_create();
	}

	~lock()
	{
		lock_destroy(var);
	}

	lock(const lock &) = delete;

	void take() { lock_wait(var); }
	void release() { lock_unlock(var); }
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

	scope_lock(const scope_lock &) = delete;
};

#endif // BASE_TL_THREADING_H
