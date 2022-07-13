#ifndef ENGINE_SHARED_FIFO_H
#define ENGINE_SHARED_FIFO_H

#include <engine/console.h>

#if defined(CONF_FAMILY_UNIX)

class CFifo
{
	IConsole *m_pConsole = nullptr;
	char m_aFilename[IO_MAX_PATH_LENGTH] = {0};
	int m_Flag = 0;
	int m_File = 0;

public:
	void Init(IConsole *pConsole, char *pFifoFile, int Flag);
	void Update();
	void Shutdown();
};

#endif

#endif
