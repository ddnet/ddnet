/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "sphore.h"

#include "dbg.h"

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>

#include <limits>
#elif defined(CONF_PLATFORM_MACOS)
#include "process.h"
#include "str.h"

#include <fcntl.h> // O_* constants
#include <sys/stat.h> // S_* constants
#elif defined(CONF_FAMILY_UNIX)
#include "str.h"

#include <cerrno>
#endif

#if defined(CONF_FAMILY_WINDOWS)
void sphore_init(SEMAPHORE *sem)
{
	*sem = CreateSemaphoreW(nullptr, 0, std::numeric_limits<LONG>::max(), nullptr);
	dbg_assert(*sem != nullptr, "CreateSemaphoreW failure");
}
void sphore_wait(SEMAPHORE *sem)
{
	dbg_assert(WaitForSingleObject((HANDLE)*sem, INFINITE) == WAIT_OBJECT_0, "WaitForSingleObject failure");
}
void sphore_signal(SEMAPHORE *sem)
{
	dbg_assert(ReleaseSemaphore((HANDLE)*sem, 1, nullptr), "ReleaseSemaphore failure");
}
void sphore_destroy(SEMAPHORE *sem)
{
	dbg_assert(CloseHandle((HANDLE)*sem), "CloseHandle failure");
}
#elif defined(CONF_PLATFORM_MACOS)
void sphore_init(SEMAPHORE *sem)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "/%d.%p", process_id(), (void *)sem);
	*sem = sem_open(aBuf, O_CREAT | O_EXCL, S_IRWXU | S_IRWXG, 0);
	dbg_assert(*sem != SEM_FAILED, "sem_open failure, errno=%d, name='%s'", errno, aBuf);
}
void sphore_wait(SEMAPHORE *sem)
{
	while(true)
	{
		if(sem_wait(*sem) == 0)
			break;
		dbg_assert(errno == EINTR, "sem_wait failure");
	}
}
void sphore_signal(SEMAPHORE *sem)
{
	dbg_assert(sem_post(*sem) == 0, "sem_post failure");
}
void sphore_destroy(SEMAPHORE *sem)
{
	dbg_assert(sem_close(*sem) == 0, "sem_close failure");
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "/%d.%p", process_id(), (void *)sem);
	dbg_assert(sem_unlink(aBuf) == 0, "sem_unlink failure");
}
#elif defined(CONF_FAMILY_UNIX)
void sphore_init(SEMAPHORE *sem)
{
	dbg_assert(sem_init(sem, 0, 0) == 0, "sem_init failure");
}
void sphore_wait(SEMAPHORE *sem)
{
	while(true)
	{
		if(sem_wait(sem) == 0)
			break;
		dbg_assert(errno == EINTR, "sem_wait failure");
	}
}
void sphore_signal(SEMAPHORE *sem)
{
	dbg_assert(sem_post(sem) == 0, "sem_post failure");
}
void sphore_destroy(SEMAPHORE *sem)
{
	dbg_assert(sem_destroy(sem) == 0, "sem_destroy failure");
}
#endif
