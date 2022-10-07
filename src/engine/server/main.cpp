
#define _WIN32_WINNT 0x0501

#include <base/logger.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/server/antibot.h>
#include <engine/server/databases/connection.h>
#include <engine/server/server.h>
#include <engine/server/server_logger.h>

#include <engine/shared/assertion_logger.h>
#include <engine/shared/config.h>

#include <game/version.h>

#include <vector>

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <csignal>

volatile sig_atomic_t InterruptSignaled = 0;

bool IsInterrupted()
{
	return InterruptSignaled;
}

void HandleSigIntTerm(int Param)
{
	InterruptSignaled = 1;

	// Exit the next time a signal is received
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	bool Silent = false;

	for(int i = 1; i < argc; i++)
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0)
		{
			Silent = true;
#if defined(CONF_FAMILY_WINDOWS)
			ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
			break;
		}
	}

#if defined(CONF_FAMILY_WINDOWS)
	CWindowsComLifecycle WindowsComLifecycle(false);
#endif

	std::vector<std::shared_ptr<ILogger>> vpLoggers;
#if defined(CONF_PLATFORM_ANDROID)
	vpLoggers.push_back(std::shared_ptr<ILogger>(log_logger_android()));
#else
	if(!Silent)
	{
		vpLoggers.push_back(std::shared_ptr<ILogger>(log_logger_stdout()));
	}
#endif
	std::shared_ptr<CFutureLogger> pFutureFileLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureFileLogger);
	std::shared_ptr<CFutureLogger> pFutureConsoleLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureConsoleLogger);
	std::shared_ptr<CFutureLogger> pFutureAssertionLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureAssertionLogger);
	log_set_global_logger(log_logger_collection(std::move(vpLoggers)).release());

	if(secure_random_init() != 0)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}
	if(MysqlInit() != 0)
	{
		dbg_msg("mysql", "failed to initialize MySQL library");
		return -1;
	}

	signal(SIGINT, HandleSigIntTerm);
	signal(SIGTERM, HandleSigIntTerm);

#if defined(CONF_EXCEPTION_HANDLING)
	init_exception_handler();
#endif

	CServer *pServer = CreateServer();
	IKernel *pKernel = IKernel::Create();

	// create the components
	IEngine *pEngine = CreateEngine(GAME_NAME, pFutureConsoleLogger, 2);
	IEngineMap *pEngineMap = CreateEngineMap();
	IGameServer *pGameServer = CreateGameServer();
	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON);
	IStorage *pStorage = CreateStorage(IStorage::STORAGETYPE_SERVER, argc, argv);
	IConfigManager *pConfigManager = CreateConfigManager();
	IEngineAntibot *pEngineAntibot = CreateEngineAntibot();

	pFutureAssertionLogger->Set(CreateAssertionLogger(pStorage, GAME_NAME));
#if defined(CONF_EXCEPTION_HANDLING)
	char aBuf[IO_MAX_PATH_LENGTH];
	char aBufName[IO_MAX_PATH_LENGTH];
	char aDate[64];
	str_timestamp(aDate, sizeof(aDate));
	str_format(aBufName, sizeof(aBufName), "dumps/" GAME_NAME "-Server_crash_log_%d_%s.RTP", pid(), aDate);
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, aBufName, aBuf, sizeof(aBuf));
	set_exception_handler_log_file(aBuf);
#endif

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pServer);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineMap); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pGameServer);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineAntibot);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IAntibot *>(pEngineAntibot), false);

		if(RegisterFail)
		{
			delete pKernel;
			return -1;
		}
	}

	pEngine->Init();
	pConfigManager->Init();
	pConsole->Init();

	// register all console commands
	pServer->RegisterCommands();

	// execute autoexec file
	IOHANDLE File = pStorage->OpenFile(AUTOEXEC_SERVER_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		pConsole->ExecuteFile(AUTOEXEC_SERVER_FILE);
	}
	else // fallback
	{
		pConsole->ExecuteFile(AUTOEXEC_FILE);
	}

	// parse the command line arguments
	if(argc > 1)
		pConsole->ParseArguments(argc - 1, &argv[1]);

	pConsole->Register("sv_test_cmds", "", CFGFLAG_SERVER, CServer::ConTestingCommands, pConsole, "Turns testing commands aka cheats on/off (setting only works in initial config)");
	pConsole->Register("sv_rescue", "", CFGFLAG_SERVER, CServer::ConRescue, pConsole, "Allow /rescue command so players can teleport themselves out of freeze (setting only works in initial config)");

	log_set_loglevel((LEVEL)g_Config.m_Loglevel);
	if(g_Config.m_Logfile[0])
	{
		IOHANDLE Logfile = io_open(g_Config.m_Logfile, IOFLAG_WRITE);
		if(Logfile)
		{
			pFutureFileLogger->Set(log_logger_file(Logfile));
		}
		else
		{
			dbg_msg("client", "failed to open '%s' for logging", g_Config.m_Logfile);
		}
	}
	pEngine->SetAdditionalLogger(std::make_unique<CServerLogger>(pServer));

	// run the server
	dbg_msg("server", "starting...");
	int Ret = pServer->Run();

	MysqlUninit();
	secure_random_uninit();

	// free
	delete pKernel;

	return Ret;
}
