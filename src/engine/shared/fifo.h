#ifndef ENGINE_SHARED_FIFO_H
#define ENGINE_SHARED_FIFO_H

#include <engine/console.h>

#if defined(CONF_FAMILY_UNIX)

#include <stdio.h> // FILE

class CFifo
{
	IConsole *m_pConsole;
	int m_Flag;
	FILE *m_File;

public:
	void Init(IConsole *pConsole, char *pFifoFile, int Flag);
	void Update();
	void Shutdown();
};

#endif

#endif
