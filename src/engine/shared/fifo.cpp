#include "fifo.h"

#if defined(CONF_FAMILY_UNIX)

#include <engine/shared/config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

void CFifo::Init(IConsole *pConsole, char *pFifoFile, int Flag)
{
	m_File = NULL;

	m_pConsole = pConsole;
	if(pFifoFile[0] == '\0')
		return;

	m_Flag = Flag;

	mkfifo(pFifoFile, 0600);

	struct stat attribute;
	stat(pFifoFile, &attribute);

	if(!S_ISFIFO(attribute.st_mode))
	{
		dbg_msg("fifo", "'%s' is not a fifo, removing", pFifoFile);
		fs_remove(pFifoFile);
		mkfifo(pFifoFile, 0600);
		stat(pFifoFile, &attribute);

		if(!S_ISFIFO(attribute.st_mode))
		{
			dbg_msg("fifo", "can't remove file '%s', quitting", pFifoFile);
			exit(2);
		}
	}

	int fileFD = open(pFifoFile, O_RDONLY|O_NONBLOCK);
	if(fileFD >= 0)
		m_File = fdopen(fileFD, "r");
	if(m_File == NULL)
		dbg_msg("fifo", "can't open file '%s'", pFifoFile);
}

void CFifo::Shutdown()
{
	fclose(m_File);
}

void CFifo::Update()
{
	if(m_File == NULL)
		return;

	char aBuf[8192];

	while(true)
	{
		char *pResult = fgets(aBuf, sizeof(aBuf), m_File);
		if(pResult == NULL) break;
		m_pConsole->ExecuteLineFlag(pResult, m_Flag, -1);
	}
}
#endif
