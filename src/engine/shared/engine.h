#ifndef ENGINE_SHARED_ENGINE_H
#define ENGINE_SHARED_ENGINE_H

#include <engine/engine.h>

class CEngine : public IEngine
{
public:
	IConsole *m_pConsole;
	IStorage *m_pStorage;
	bool m_Logging;

	std::shared_ptr<CFutureLogger> m_pFutureLogger;

	char m_aAppName[256];

	CEngine(bool Test, const char *pAppname, std::shared_ptr<CFutureLogger> pFutureLogger, int Jobs);
	virtual ~CEngine() override;

	virtual void Init() override;
	virtual void AddJob(std::shared_ptr<IJob> pJob) override;
	virtual void SetAdditionalLogger(std::unique_ptr<ILogger> &&pLogger) override;
};

IEngine *CreateEngine(const char *pAppname, std::shared_ptr<CFutureLogger> pFutureLogger, int Jobs) { return new CEngine(false, pAppname, std::move(pFutureLogger), Jobs); }
IEngine *CreateTestEngine(const char *pAppname, int Jobs) { return new CEngine(true, pAppname, nullptr, Jobs); }

#endif //ENGINE_SHARED_ENGINE_H
