/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_ENGINE_H
#define ENGINE_ENGINE_H

#include <memory>

#include "kernel.h"

class CFutureLogger;
class ILogger;

class IEngineRunnable
{
public:
	virtual int Status() = 0;
	virtual void Run() = 0;
	virtual int Runner() = 0;
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
