#include "fifoconsole.h"

#if defined(CONF_FAMILY_UNIX)

#include <engine/shared/config.h>

#include <fstream>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

static LOCK gs_FifoLock = 0;
static volatile bool gs_stopFifoThread = false;

FifoConsole::FifoConsole(IConsole *pConsole, char *pFifoFile, int flag)
{
	m_pFifoFile = pFifoFile;
	if(m_pFifoFile[0] == '\0')
		return;

	m_pFifoThread = thread_init(ListenFifoThread, this);
	m_pConsole = pConsole;
	m_flag = flag;

	gs_stopFifoThread = false;
	if(gs_FifoLock == 0)
		gs_FifoLock = lock_create();

	pthread_detach((pthread_t)m_pFifoThread);
}

FifoConsole::~FifoConsole()
{
	if(m_pFifoFile[0] == '\0')
		return;

	lock_wait(gs_FifoLock);
	gs_stopFifoThread = true;
	lock_unlock(gs_FifoLock);
	gs_FifoLock = 0;
}

void FifoConsole::ListenFifoThread(void *pUser)
{
	FifoConsole *pData = (FifoConsole *)pUser;

	if(!gs_FifoLock)
	{
		dbg_msg("fifo", "FIFO not properly initialized");
		exit(2);
	}

	lock_wait(gs_FifoLock);
	if(gs_stopFifoThread)
		return;

	mkfifo(pData->m_pFifoFile, 0600);

	struct stat attribute;
	stat(pData->m_pFifoFile, &attribute);

	if(!S_ISFIFO(attribute.st_mode))
	{
		dbg_msg("fifo", "'%s' is not a FIFO, removing", pData->m_pFifoFile);
		fs_remove(pData->m_pFifoFile);
		mkfifo(pData->m_pFifoFile, 0600);
		stat(pData->m_pFifoFile, &attribute);

		if(!S_ISFIFO(attribute.st_mode))
		{
			dbg_msg("fifo", "Can't remove file, quitting");
			exit(2);
		}
	}

	lock_unlock(gs_FifoLock);

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
