#include "fifoconsole.h"

#include <engine/shared/config.h>

#include <fstream>

#if defined(CONF_FAMILY_UNIX)

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

static volatile bool stopFifoThread = false;

FifoConsole::FifoConsole(IConsole *pConsole, char *pFifoFile, int flag)
{
	void *m_pFifoThread = thread_create(ListenFifoThread, this);
	m_pConsole = pConsole;
	m_pFifoFile = pFifoFile;
	m_flag = flag;

	stopFifoThread = false;

	pthread_detach((pthread_t)m_pFifoThread);
}

FifoConsole::~FifoConsole()
{
	stopFifoThread = true;
	//pthread_wait((pthread_t)m_pFifoThread, NULL);
}

void FifoConsole::ListenFifoThread(void *pUser)
{
	FifoConsole *pData = (FifoConsole *)pUser;

	// This should fix the problem where sometimes the fifo thread runs at a bad
	// time and can't open the fifo immediately.
	while (!pData->m_pFifoFile || str_comp(pData->m_pFifoFile, "") == 0)
	{
		thread_sleep(1000);

		if(stopFifoThread)
			return;
	}

	mkfifo(pData->m_pFifoFile, 0600);

	struct stat attribute;
	stat(pData->m_pFifoFile, &attribute);

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
