#ifndef ENGINE_SHARED_ASSERTION_LOGGER_H
#define ENGINE_SHARED_ASSERTION_LOGGER_H

#include <base/system.h>

#include <cstddef>
#include <mutex>

#include <engine/shared/ringbuffer.h>

class CAssertionLogger
{
	static void DbgLogger(const char *pLine, void *pUser);
	static void DbgLoggerAssertion(void *pUser);

	static constexpr size_t s_MaxDbgMessageCount = 64;

	void DbgLogger(const char *pLine);
	void DbgLoggerAssertion();

	struct SDebugMessageItem
	{
		char m_aMessage[1024];
	};

	std::mutex m_DbgMessageMutex;
	CStaticRingBuffer<SDebugMessageItem, sizeof(SDebugMessageItem) * s_MaxDbgMessageCount, CRingBufferBase::FLAG_RECYCLE> m_DbgMessages;

	char m_aAssertLogPath[IO_MAX_PATH_LENGTH];
	char m_aGameName[256];

public:
	void Init(const char *pAssertLogPath, const char *pGameName);
};

#endif
