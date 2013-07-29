#ifndef ENGINE_FIFOCONSOLE_H
#define ENGINE_FIFOCONSOLE_H

#include <engine/console.h>

class FifoConsole
{
	static void ListenFifoThread(void *pUser);

public:
	FifoConsole(IConsole *pConsole);
};

#endif // FILE_ENGINE_FIFOCONSOLE_H
