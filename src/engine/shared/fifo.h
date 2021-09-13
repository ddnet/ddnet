#ifndef ENGINE_SHARED_FIFO_H
#define ENGINE_SHARED_FIFO_H

#include <engine/console.h>

#if defined(CONF_FAMILY_UNIX)

class CFifo
{
	IConsole *m_pConsole;
	char m_aFilename[IO_MAX_PATH_LENGTH];
	int m_Flag;
	int m_File;

public:
	void Init(IConsole *pConsole, char *pFifoFile, int Flag);
	void Update();
	void Shutdown();
};

#endif

#endif
