#ifndef ENGINE_FIFOCONSOLE_H
#define ENGINE_FIFOCONSOLE_H

#include <engine/console.h>

#if defined(CONF_FAMILY_UNIX)

class FifoConsole
{
	static void ListenFifoThread(void *pUser);
	IConsole *m_pConsole;
	void *m_pFifoThread;
	char *m_pFifoFile;
	int m_flag;

public:
	FifoConsole(IConsole *pConsole, char *pFifoFile, int flag);
	~FifoConsole();
};
#endif

#endif // FILE_ENGINE_FIFOCONSOLE_H
