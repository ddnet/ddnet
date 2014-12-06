#include "fifoconsole.h"

#include <engine/shared/config.h>

#include <fstream>

#if defined(CONF_FAMILY_UNIX)

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

static LOCK gs_FifoLock = 0;
static volatile bool gs_stopFifoThread = false;

FifoConsole::FifoConsole(IConsole *pConsole, char *pFifoFile, int flag)
{
	void *m_pFifoThread = thread_create(ListenFifoThread, this);
	m_pConsole = pConsole;
	m_pFifoFile = pFifoFile;
	m_flag = flag;

	gs_stopFifoThread = false;
	if(gs_FifoLock == 0)
		gs_FifoLock = lock_create();

	pthread_detach((pthread_t)m_pFifoThread);
}

FifoConsole::~FifoConsole()
{
	lock_wait(gs_FifoLock);
	gs_stopFifoThread = true;
	lock_release(gs_FifoLock);
	gs_FifoLock = 0;
}

void FifoConsole::ListenFifoThread(void *pUser)
{
	FifoConsole *pData = (FifoConsole *)pUser;

	if(!gs_FifoLock)
		return;
	lock_wait(gs_FifoLock);
	if(gs_stopFifoThread)
		return;

	// This should fix the problem where sometimes the fifo thread runs at a bad
	// time and can't open the fifo immediately.
	while (!pData->m_pFifoFile || str_comp(pData->m_pFifoFile, "") == 0)
	{
		thread_sleep(1000);
	}

	mkfifo(pData->m_pFifoFile, 0600);

	struct stat attribute;
	stat(pData->m_pFifoFile, &attribute);

	lock_release(gs_FifoLock);

	if(!S_ISFIFO(attribute.st_mode))
		return;

	std::ifstream f;
	char aBuf[8192];

	while (true)
	{
		f.open(pData->m_pFifoFile);
		while (f.getline(aBuf, sizeof(aBuf)))
			pData->m_pConsole->ExecuteLineFlag(aBuf, pData->m_flag, -1);
		f.close();
	}
}
#endif
