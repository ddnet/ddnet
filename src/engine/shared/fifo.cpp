#include "fifo.h"

#include <base/system.h>
#if defined(CONF_FAMILY_UNIX)

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void CFifo::Init(IConsole *pConsole, const char *pFifoFile, int Flag)
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
			dbg_msg("fifo", "can't remove file '%s'", m_aFilename);
			return;
		}
	}

	m_File = open(m_aFilename, O_RDONLY | O_NONBLOCK);
	if(m_File < 0)
		dbg_msg("fifo", "can't open file '%s'", m_aFilename);
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
			m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
		}
		pCur = aBuf + i + 1;
	}
	if(pCur < aBuf + Length && str_utf8_check(pCur)) // missed the last line
	{
		m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
	}
}

#elif defined(CONF_FAMILY_WINDOWS)

#include <windows.h>

void CFifo::Init(IConsole *pConsole, const char *pFifoFile, int Flag)
{
	m_pConsole = pConsole;
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
		const std::string ErrorMsg = windows_format_system_message(LastError);
		dbg_msg("fifo", "failed to create named pipe '%s' (%ld %s)", m_aFilename, LastError, ErrorMsg.c_str());
	}
	else
		dbg_msg("fifo", "created named pipe '%s'", m_aFilename);
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
			const std::string ErrorMsg = windows_format_system_message(LastError);
			dbg_msg("fifo", "failed to connect named pipe '%s' (%ld %s)", m_aFilename, LastError, ErrorMsg.c_str());
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
				const std::string ErrorMsg = windows_format_system_message(LastError);
				dbg_msg("fifo", "failed to peek at pipe '%s' (%ld %s)", m_aFilename, LastError, ErrorMsg.c_str());
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
			const std::string ErrorMsg = windows_format_system_message(LastError);
			dbg_msg("fifo", "failed to read from pipe '%s' (%ld %s)", m_aFilename, LastError, ErrorMsg.c_str());
			free(pBuf);
			return;
		}
		pBuf[Length] = '\0';

		char *pCur = pBuf;
		for(DWORD i = 0; i < Length; ++i)
		{
			if(pBuf[i] != '\n')
				continue;
			pBuf[i] = '\0';
			if(str_utf8_check(pCur))
			{
				m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
			}
			pCur = pBuf + i + 1;
		}
		if(pCur < pBuf + Length && str_utf8_check(pCur)) // missed the last line
		{
			m_pConsole->ExecuteLineFlag(pCur, m_Flag, -1);
		}

		free(pBuf);
	}
}
#endif
