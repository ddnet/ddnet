#ifndef ENGINE_SHARED_FIFO_H
#define ENGINE_SHARED_FIFO_H

#include <base/detect.h>
#include <engine/console.h>

class CFifo
{
	IConsole *m_pConsole;
	char m_aFilename[IO_MAX_PATH_LENGTH];
	int m_Flag;
#if defined(CONF_FAMILY_UNIX)
	int m_File;
#elif defined(CONF_FAMILY_WINDOWS)
	void *m_pPipe;
#endif

public:
	void Init(IConsole *pConsole, const char *pFifoFile, int Flag);
	void Update();
	void Shutdown();
};

#endif
