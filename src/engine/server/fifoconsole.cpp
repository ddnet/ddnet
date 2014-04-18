#include "fifoconsole.h"

#include <engine/shared/config.h>

#include <fstream>

#if defined(CONF_FAMILY_UNIX)
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

FifoConsole::FifoConsole(IConsole *pConsole)
{
	void *m_pFifoThread = thread_create(ListenFifoThread, pConsole);
	pthread_detach((pthread_t)m_pFifoThread);
}

void FifoConsole::ListenFifoThread(void *pUser)
{
	IConsole *pConsole = (IConsole *)pUser;

	if (str_comp(g_Config.m_SvInputFifo, "") == 0)
		return;

	mkfifo(g_Config.m_SvInputFifo, 0600);

	struct stat attribute;
	stat(g_Config.m_SvInputFifo, &attribute);

	if(!S_ISFIFO(attribute.st_mode))
		return;

	std::ifstream f;
	char aBuf[8192];

	while (true)
	{
		f.open(g_Config.m_SvInputFifo);
		while (f.getline(aBuf, sizeof(aBuf)))
		{
			pConsole->ExecuteLineFlag(aBuf, CFGFLAG_SERVER, -1);
		}
		f.close();
	}
}
#endif
