#ifndef ENGINE_FIFOCONSOLE_H
#define ENGINE_FIFOCONSOLE_H

#include <engine/console.h>

class FifoConsole
{
	static void ListenFifoThread(void *pUser);
	void *m_pFifoThread;

public:
	FifoConsole(IConsole *pConsole);
	~FifoConsole();
};

#endif // FILE_ENGINE_FIFOCONSOLE_H
