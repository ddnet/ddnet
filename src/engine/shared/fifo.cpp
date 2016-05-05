#include "fifo.h"

#include <base/system.h>
#if defined(CONF_FAMILY_UNIX)

#include <engine/shared/config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

void CFifo::Init(IConsole *pConsole, char *pFifoFile, int Flag)
{
	m_File = -1;

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

	m_File = open(pFifoFile, O_RDONLY|O_NONBLOCK);
	if(m_File < 0)
		dbg_msg("fifo", "can't open file '%s'", pFifoFile);
}

void CFifo::Shutdown()
{
	if(m_File >= 0)
		close(m_File);
}

void CFifo::Update()
{
	if(m_File < 0)
		return;

	char aBuf[8192];
	int Length = read(m_File, aBuf, sizeof(aBuf));
	if(Length <= 0)
		return;

	char *pCur = aBuf;
	for(int i = 0; i < Length; ++i)
	{
		if(aBuf[i] != '\n')
			continue;
		aBuf[i] = '\0';
		m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
		pCur = aBuf+i+1;
	}
	if(pCur < aBuf+Length) // missed the last line
		m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
}
#endif
