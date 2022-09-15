#ifndef BASE_LOCK_SCOPE_H
#define BASE_LOCK_SCOPE_H

#include "system.h"

class SCOPED_CAPABILITY CLockScope
{
public:
	CLockScope(LOCK Lock) ACQUIRE(Lock, m_Lock) :
		m_Lock(Lock)
	{
		lock_wait(m_Lock);
	}

	~CLockScope() RELEASE() REQUIRES(m_Lock)
	{
		lock_unlock(m_Lock);
	}

private:
	LOCK m_Lock;
};

#endif
