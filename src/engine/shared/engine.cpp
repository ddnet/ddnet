/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/logger.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
#include <engine/shared/network.h>
#include <engine/storage.h>

#include <thread>

class CEngine : public IEngine
{
	IConsole *m_pConsole;
	IStorage *m_pStorage;

	bool m_Logging;
	std::shared_ptr<CFutureLogger> m_pFutureLogger;

	char m_aAppName[256];

	CJobPool m_JobPool;

	static void Con_DbgLognetwork(IConsole::IResult *pResult, void *pUserData)
	{
		CEngine *pEngine = static_cast<CEngine *>(pUserData);

		if(pEngine->m_Logging)
		{
			CNetBase::CloseLog();
			pEngine->m_Logging = false;
		}
		else
		{
			char aBuf[32];
			str_timestamp(aBuf, sizeof(aBuf));
			char aFilenameSent[IO_MAX_PATH_LENGTH], aFilenameRecv[IO_MAX_PATH_LENGTH];
			str_format(aFilenameSent, sizeof(aFilenameSent), "dumps/network_sent_%s.txt", aBuf);
			str_format(aFilenameRecv, sizeof(aFilenameRecv), "dumps/network_recv_%s.txt", aBuf);
			CNetBase::OpenLog(pEngine->m_pStorage->OpenFile(aFilenameSent, IOFLAG_WRITE, IStorage::TYPE_SAVE),
				pEngine->m_pStorage->OpenFile(aFilenameRecv, IOFLAG_WRITE, IStorage::TYPE_SAVE));
			pEngine->m_Logging = true;
		}
	}

public:
	CEngine(bool Test, const char *pAppname, std::shared_ptr<CFutureLogger> pFutureLogger) :
		m_pFutureLogger(std::move(pFutureLogger))
	{
		str_copy(m_aAppName, pAppname);
		if(!Test)
		{
			log_info("engine", "running on %s-%s-%s", CONF_FAMILY_STRING, CONF_PLATFORM_STRING, CONF_ARCH_STRING);
			log_info("engine", "arch is %s", CONF_ARCH_ENDIAN_STRING);

			char aVersionStr[128];
			if(os_version_str(aVersionStr, sizeof(aVersionStr)))
			{
				log_info("engine", "operating system version: %s", aVersionStr);
			}

			// init the network
			net_init();
			CNetBase::Init();
		}

#if defined(CONF_PLATFORM_EMSCRIPTEN)
		// Make sure we don't use more threads than allowed in total (see PTHREAD_POOL_SIZE in Emscripten.toolchain)
		// otherwise starting more threads may lead to deadlocks as the threads will simply not start.
		const size_t ThreadCount = 4;
#else
		const size_t ThreadCount = std::max(4, (int)std::thread::hardware_concurrency()) - 2;
#endif
		m_JobPool.Init(ThreadCount);

		m_Logging = false;
	}

	~CEngine() override
	{
		CNetBase::CloseLog();
	}

	void Init() override
	{
		m_pConsole = Kernel()->RequestInterface<IConsole>();
		m_pStorage = Kernel()->RequestInterface<IStorage>();

		if(!m_pConsole || !m_pStorage)
			return;

		m_pConsole->Register("dbg_lognetwork", "", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_DbgLognetwork, this, "Log the network");
	}

	void AddJob(std::shared_ptr<IJob> pJob) override
	{
		m_JobPool.Add(std::move(pJob));
	}

	void ShutdownJobs() override
	{
		m_JobPool.Shutdown();
	}

	void SetAdditionalLogger(std::shared_ptr<ILogger> &&pLogger) override
	{
		m_pFutureLogger->Set(pLogger);
	}
};

IEngine *CreateEngine(const char *pAppname, std::shared_ptr<CFutureLogger> pFutureLogger) { return new CEngine(false, pAppname, std::move(pFutureLogger)); }
IEngine *CreateTestEngine(const char *pAppname) { return new CEngine(true, pAppname, nullptr); }
