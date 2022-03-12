#include "assertion_logger.h"

#include <base/system.h>
#include <mutex>

void CAssertionLogger::DbgLogger(const char *pLine, void *pUser)
{
	((CAssertionLogger *)pUser)->DbgLogger(pLine);
}

void CAssertionLogger::DbgLoggerAssertion(void *pUser)
{
	((CAssertionLogger *)pUser)->DbgLoggerAssertion();
}

void CAssertionLogger::DbgLogger(const char *pLine)
{
	std::unique_lock<std::mutex> Lock(m_DbgMessageMutex);

	SDebugMessageItem *pMsgItem = (SDebugMessageItem *)m_DbgMessages.Allocate(sizeof(SDebugMessageItem));
	str_copy(pMsgItem->m_aMessage, pLine, std::size(pMsgItem->m_aMessage));
}

void CAssertionLogger::DbgLoggerAssertion()
{
	char aAssertLogFile[IO_MAX_PATH_LENGTH];
	char aDate[64];
	str_timestamp(aDate, sizeof(aDate));
	str_format(aAssertLogFile, std::size(aAssertLogFile), "%s%s_assert_log_%d_%s.txt", m_aAssertLogPath, m_aGameName, pid(), aDate);
	std::unique_lock<std::mutex> Lock(m_DbgMessageMutex);
	IOHANDLE FileHandle = io_open(aAssertLogFile, IOFLAG_WRITE);
	if(FileHandle)
	{
		auto *pIt = m_DbgMessages.First();
		while(pIt)
		{
			io_write(FileHandle, pIt->m_aMessage, str_length(pIt->m_aMessage));
			io_write(FileHandle, "\n", 1);

			pIt = m_DbgMessages.Next(pIt);
		}

		io_sync(FileHandle);
		io_close(FileHandle);
	}
}

void CAssertionLogger::Init(const char *pAssertLogPath, const char *pGameName)
{
	str_copy(m_aAssertLogPath, pAssertLogPath, std::size(m_aAssertLogPath));
	str_copy(m_aGameName, pGameName, std::size(m_aGameName));

	dbg_logger_assertion(DbgLogger, nullptr, DbgLoggerAssertion, this);
}
