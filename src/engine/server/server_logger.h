#ifndef ENGINE_SERVER_SERVER_LOGGER_H
#define ENGINE_SERVER_SERVER_LOGGER_H
#include <base/logger.h>

#include <thread>

class CServer;

class CServerLogger : public ILogger
{
	CServer *m_pServer = nullptr;
	std::mutex m_PendingLock;
	std::vector<CLogMessage> m_vPending;
	std::thread::id m_MainThread;

public:
	CServerLogger(CServer *pServer);
	void Log(const CLogMessage *pMessage) override;
	// Must be called from the main thread!
	void OnServerDeletion();
};

#endif // ENGINE_SERVER_SERVER_LOGGER_H
