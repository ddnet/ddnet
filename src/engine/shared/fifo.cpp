#include "fifo.h"

#include <base/log.h>
#include <base/system.h>
#if defined(CONF_FAMILY_UNIX)

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>

void CFifo::Init(IConsole *pConsole, const char *pFifoFile, int Flag)
{
	m_File = -1;

	m_pConsole = pConsole;
	m_IsInit = true;
	if(pFifoFile[0] == '\0')
		return;

	str_copy(m_aFilename, pFifoFile);
	m_Flag = Flag;

	bool IsFifo;
	if(mkfifo(m_aFilename, 0600) == 0)
	{
		struct stat Attribute;
		if(stat(m_aFilename, &Attribute) == 0)
		{
			IsFifo = S_ISFIFO(Attribute.st_mode);
		}
		else
		{
			log_warn("fifo", "Failed to stat fifo '%s' (%d '%s')", m_aFilename, errno, strerror(errno));
			IsFifo = false;
		}
	}
	else
	{
		log_warn("fifo", "Failed to create fifo '%s' (%d '%s')", m_aFilename, errno, strerror(errno));
		IsFifo = false;
	}
	if(!IsFifo)
	{
		log_warn("fifo", "File '%s' is not a fifo, removing", m_aFilename);
		if(fs_remove(m_aFilename) != 0)
		{
			log_error("fifo", "Failed to remove non-fifo '%s'", m_aFilename); // details logged in fs_remove
			return;
		}
		if(mkfifo(m_aFilename, 0600) != 0)
		{
			log_error("fifo", "Failed to create fifo '%s' (%d '%s')", m_aFilename, errno, strerror(errno));
			return;
		}
		struct stat Attribute;
		if(stat(m_aFilename, &Attribute) != 0)
		{
			log_error("fifo", "Failed to stat fifo '%s' (%d '%s')", m_aFilename, errno, strerror(errno));
			return;
		}
		if(!S_ISFIFO(Attribute.st_mode))
		{
			log_error("fifo", "File '%s' is not a fifo", m_aFilename);
			return;
		}
	}

	m_File = open(m_aFilename, O_RDONLY | O_NONBLOCK);
	if(m_File < 0)
	{
		log_error("fifo", "Failed to open fifo '%s' (%d '%s')", m_aFilename, errno, strerror(errno));
	}
	else
	{
		log_info("fifo", "Opened fifo '%s'", m_aFilename);
	}
}

void CFifo::Shutdown()
{
	if(m_File < 0)
		return;

	close(m_File);
	fs_remove(m_aFilename);
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
		if(str_utf8_check(pCur))
		{
			m_pConsole->ExecuteLineFlag(pCur, m_Flag, IConsole::CLIENT_ID_UNSPECIFIED);
		}
		pCur = aBuf + i + 1;
	}
	if(pCur < aBuf + Length && str_utf8_check(pCur)) // missed the last line
	{
		m_pConsole->ExecuteLineFlag(pCur, m_Flag, IConsole::CLIENT_ID_UNSPECIFIED);
	}
}

#elif defined(CONF_FAMILY_WINDOWS)

#include <base/windows.h>

#include <windows.h>

void CFifo::Init(IConsole *pConsole, const char *pFifoFile, int Flag)
{
	m_pConsole = pConsole;
	m_IsInit = true;
	if(pFifoFile[0] == '\0')
	{
		m_pPipe = INVALID_HANDLE_VALUE;
		return;
	}

	str_copy(m_aFilename, "\\\\.\\pipe\\");
	str_append(m_aFilename, pFifoFile);
	m_Flag = Flag;

	const std::wstring WideFilename = windows_utf8_to_wide(m_aFilename);
	m_pPipe = CreateNamedPipeW(WideFilename.c_str(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
		PIPE_UNLIMITED_INSTANCES,
		8192,
		8192,
		NMPWAIT_USE_DEFAULT_WAIT,
		nullptr);
	if(m_pPipe == INVALID_HANDLE_VALUE)
	{
		const DWORD LastError = GetLastError();
		log_error("fifo", "Failed to create named pipe '%s' (%ld '%s')", m_aFilename, LastError, windows_format_system_message(LastError).c_str());
	}
	else
	{
		log_info("fifo", "Created named pipe '%s'", m_aFilename);
	}
}

void CFifo::Shutdown()
{
	if(m_pPipe == INVALID_HANDLE_VALUE)
		return;

	DisconnectNamedPipe(m_pPipe);
	CloseHandle(m_pPipe);
	m_pPipe = INVALID_HANDLE_VALUE;
}

void CFifo::Update()
{
	if(m_pPipe == INVALID_HANDLE_VALUE)
		return;

	if(!ConnectNamedPipe(m_pPipe, nullptr))
	{
		const DWORD LastError = GetLastError();
		if(LastError == ERROR_PIPE_LISTENING) // waiting for clients to connect
			return;
		if(LastError == ERROR_NO_DATA) // pipe was disconnected from the other end, also disconnect it from this end
		{
			// disconnect the previous client so we can connect to a new one
			DisconnectNamedPipe(m_pPipe);
			return;
		}
		if(LastError != ERROR_PIPE_CONNECTED) // pipe already connected, not an error
		{
			log_error("fifo", "Failed to connect named pipe '%s' (%ld '%s')", m_aFilename, LastError, windows_format_system_message(LastError).c_str());
			return;
		}
	}

	while(true) // read all messages from the pipe
	{
		DWORD BytesAvailable;
		if(!PeekNamedPipe(m_pPipe, nullptr, 0, nullptr, &BytesAvailable, nullptr))
		{
			const DWORD LastError = GetLastError();
			if(LastError == ERROR_BROKEN_PIPE)
			{
				// Pipe was disconnected from the other side, either immediately
				// after connecting or after reading the previous message.
				DisconnectNamedPipe(m_pPipe);
			}
			else
			{
				log_error("fifo", "Failed to peek at pipe '%s' (%ld '%s')", m_aFilename, LastError, windows_format_system_message(LastError).c_str());
			}
			return;
		}
		if(BytesAvailable == 0) // pipe connected but no data available
			return;

		char *pBuf = static_cast<char *>(malloc(BytesAvailable + 1));
		DWORD Length;
		if(!ReadFile(m_pPipe, pBuf, BytesAvailable, &Length, nullptr))
		{
			const DWORD LastError = GetLastError();
			log_error("fifo", "Failed to read from pipe '%s' (%ld '%s')", m_aFilename, LastError, windows_format_system_message(LastError).c_str());
			free(pBuf);
			return;
		}
		pBuf[Length] = '\0';

		char *pCur = pBuf;
		for(DWORD i = 0; i < Length; ++i)
		{
			if(pBuf[i] != '\n' && (pBuf[i] != '\r' || pBuf[i + 1] != '\n'))
			{
				continue;
			}
			if(pBuf[i] == '\r')
			{
				pBuf[i] = '\0';
				++i;
			}
			pBuf[i] = '\0';
			if(pCur[0] != '\0' && str_utf8_check(pCur))
			{
				m_pConsole->ExecuteLineFlag(pCur, m_Flag, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			pCur = pBuf + i + 1;
		}
		if(pCur < pBuf + Length && pCur[0] != '\0' && str_utf8_check(pCur)) // missed the last line
		{
			m_pConsole->ExecuteLineFlag(pCur, m_Flag, IConsole::CLIENT_ID_UNSPECIFIED);
		}

		free(pBuf);
	}
}
#endif
