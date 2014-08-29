#include "fifoconsole.h"

#include <engine/shared/config.h>

#include <fstream>

#if defined(CONF_FAMILY_UNIX)

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

extern bool IsClient;

FifoConsole::FifoConsole(IConsole *pConsole)
{
	void *m_pFifoThread = thread_create(ListenFifoThread, pConsole);
	pthread_detach((pthread_t)m_pFifoThread);
}

void FifoConsole::ListenFifoThread(void *pUser)
{
	IConsole *pConsole = (IConsole *)pUser;

        char *fifofile;
	if (IsClient)
		fifofile = g_Config.m_ClInputFifo;
        else
                fifofile = g_Config.m_SvInputFifo;

	if (str_comp(fifofile, "") == 0)
		return;

	mkfifo(fifofile, 0600);

	struct stat attribute;
	stat(fifofile, &attribute);

	if(!S_ISFIFO(attribute.st_mode))
		return;

	std::ifstream f;
	char aBuf[8192];

	while (true)
	{
		f.open(fifofile);
		while (f.getline(aBuf, sizeof(aBuf)))
		{
			if (IsClient)
				pConsole->ExecuteLineFlag(aBuf, CFGFLAG_CLIENT, -1);
			else
				pConsole->ExecuteLineFlag(aBuf, CFGFLAG_SERVER, -1);
		}
		f.close();
	}
}
#endif
