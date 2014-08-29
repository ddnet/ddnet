#ifndef ENGINE_FIFOCONSOLE_H
#define ENGINE_FIFOCONSOLE_H

#include <engine/console.h>

#if defined(CONF_FAMILY_UNIX)

class FifoConsole
{
	static void ListenFifoThread(void *pUser);
	void *m_pFifoThread;

public:
	FifoConsole(IConsole *pConsole);
};
#endif

#endif // FILE_ENGINE_FIFOCONSOLE_H
