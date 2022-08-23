#include "fifo.h"

#include <base/system.h>
#if defined(CONF_FAMILY_UNIX)

#include <engine/shared/config.h>

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void CFifo::Init(IConsole *pConsole, char *pFifoFile, int Flag)
{
	m_File = -1;

	m_pConsole = pConsole;
	if(pFifoFile[0] == '\0')
		return;

	str_copy(m_aFilename, pFifoFile);
	m_Flag = Flag;

	mkfifo(m_aFilename, 0600);

	struct stat Attribute;
	stat(m_aFilename, &Attribute);

	if(!S_ISFIFO(Attribute.st_mode))
	{
		dbg_msg("fifo", "'%s' is not a fifo, removing", m_aFilename);
		fs_remove(m_aFilename);
		mkfifo(m_aFilename, 0600);
		stat(m_aFilename, &Attribute);

		if(!S_ISFIFO(Attribute.st_mode))
		{
			dbg_msg("fifo", "can't remove file '%s', quitting", m_aFilename);
			exit(2);
		}
	}

	m_File = open(m_aFilename, O_RDONLY | O_NONBLOCK);
	if(m_File < 0)
		dbg_msg("fifo", "can't open file '%s'", m_aFilename);
}

void CFifo::Shutdown()
{
	if(m_File >= 0)
	{
		close(m_File);
		fs_remove(m_aFilename);
	}
}

void CFifo::Update()
{
	if(m_File < 0)
		return;

	char aBuf[8192];
	int Length = read(m_File, aBuf, sizeof(aBuf) - 1);
	if(Length <= 0)
		return;
	aBuf[Length] = '\0';

	char *pCur = aBuf;
	for(int i = 0; i < Length; ++i)
	{
		if(aBuf[i] != '\n')
			continue;
		aBuf[i] = '\0';
		m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
		pCur = aBuf + i + 1;
	}
	if(pCur < aBuf + Length) // missed the last line
		m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
}
#endif
