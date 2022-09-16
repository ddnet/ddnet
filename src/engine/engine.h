/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_ENGINE_H
#define ENGINE_ENGINE_H

#include <atomic>
#include <condition_variable>
#include <memory>

#include "kernel.h"

class CFutureLogger;
class ILogger;

class IEngineRunnable
{
public:
	enum EStatus
	{
		PENDING = 0,
		RUNNING,
		DONE
	};

	virtual int Runner() = 0;
	virtual void Run() = 0;

private:
	std::atomic<EStatus> m_Status = PENDING;

protected:
	std::mutex m_StatusLock;
	std::condition_variable m_StatusCV;
	void SetStatus(EStatus NewStatus);

public:
	EStatus Status() { return m_Status; };
	void Wait();
};

class IEngineRunner
{
public:
	virtual void Run(std::shared_ptr<IEngineRunnable> pRunnable) = 0;
	virtual void Shutdown() = 0;
};

class IJob;
class IEngine : public IInterface
{
	MACRO_INTERFACE("engine", 0)

public:
	virtual void Shutdown() override = 0;
	virtual ~IEngine() = default;

	virtual void Init() = 0;
	virtual int RegisterRunner(IEngineRunner *pRunner) = 0;
	virtual void Dispatch(std::shared_ptr<IEngineRunnable> pRunnable) = 0;
	virtual void SetAdditionalLogger(std::unique_ptr<ILogger> &&pLogger) = 0;
	static void RunJobBlocking(IJob *pJob);
};

extern IEngine *CreateEngine(const char *pAppname, std::shared_ptr<CFutureLogger> pFutureLogger, int Jobs);
extern IEngine *CreateTestEngine(const char *pAppname, int Jobs);

#endif
