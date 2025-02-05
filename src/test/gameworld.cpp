#include "test.h"
#include <gtest/gtest.h>

#include <base/logger.h>
#include <base/system.h>
#include <engine/engine.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/register.h>
#include <engine/server/server.h>
#include <engine/server/server_logger.h>
#include <engine/shared/assertion_logger.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/version.h>

#include <thread>

bool IsInterrupted()
{
	return false;
}

std::vector<std::string> FakeQueue;
std::vector<std::string> FetchAndroidServerCommandQueue()
{
	return FakeQueue;
}

class GameWorld : public ::testing::Test
{
public:
	IGameServer *m_pGameServer = nullptr;
	CServer *m_pServer = nullptr;
	IKernel *m_pKernel;
	std::shared_ptr<CServerLogger> m_pServerLogger = nullptr;

	CGameContext *GameServer()
	{
		return (CGameContext *)m_pGameServer;
	}

	GameWorld()
	{
		std::shared_ptr<CFutureLogger> pFutureConsoleLogger = std::make_shared<CFutureLogger>();

		CServer *pServer = CreateServer();
		m_pServer = pServer;

		m_pKernel = IKernel::Create();
		m_pKernel->RegisterInterface(pServer);

		IEngine *pEngine = CreateEngine(GAME_NAME, pFutureConsoleLogger, (2 * std::thread::hardware_concurrency()) + 2);
		m_pKernel->RegisterInterface(pEngine);

		const char *apArgs[] = {"DDNet"};
		IStorage *pStorage = CreateStorage(IStorage::EInitializationType::SERVER, 1, apArgs);
		EXPECT_NE(pStorage, nullptr);
		m_pKernel->RegisterInterface(pStorage);

		IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON).release();
		m_pKernel->RegisterInterface(pConsole);

		IConfigManager *pConfigManager = CreateConfigManager();
		m_pKernel->RegisterInterface(pConfigManager);

		IEngineMap *pEngineMap = CreateEngineMap();
		m_pKernel->RegisterInterface(pEngineMap);
		m_pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);

		IEngineAntibot *pEngineAntibot = CreateEngineAntibot();
		m_pKernel->RegisterInterface(pEngineAntibot);
		m_pKernel->RegisterInterface(static_cast<IAntibot *>(pEngineAntibot), false);

		m_pGameServer = CreateGameServer();
		m_pKernel->RegisterInterface(m_pGameServer);

		pEngine->Init();
		pConsole->Init();
		pConfigManager->Init();

		pServer->RegisterCommands();

		m_pServerLogger = std::make_shared<CServerLogger>(pServer);
		pEngine->SetAdditionalLogger(m_pServerLogger);

		EXPECT_NE(pServer->LoadMap("coverage"), 0);
		pServer->Antibot()->Init();
		m_pGameServer->OnInit(nullptr);

		pServer->m_AuthManager.Init();

		{
			int Size = GameServer()->PersistentClientDataSize();
			for(auto &Client : pServer->m_aClients)
			{
				Client.m_HasPersistentData = false;
				Client.m_pPersistentData = malloc(Size);
			}
		}
		pServer->m_pPersistentData = malloc(GameServer()->PersistentDataSize());
		EXPECT_NE(pServer->LoadMap("coverage"), 0);

		if(!pServer->m_Http.Init(std::chrono::seconds{2}))
		{
			log_error("server", "Failed to initialize the HTTP client.");
		}

		pServer->m_pEngine = m_pKernel->RequestInterface<IEngine>();
		pServer->m_pRegister = CreateRegister(&g_Config, pServer->m_pConsole, pServer->m_pEngine, &pServer->m_Http, 8303, pServer->m_NetServer.GetGlobalToken());

		pServer->m_NetServer.SetCallbacks(pServer->NewClientCallback, pServer->NewClientNoAuthCallback, pServer->ClientRejoinCallback, pServer->DelClientCallback, pServer);

		pServer->m_Econ.Init(pServer->Config(), pServer->Console(), &pServer->m_ServerBan);

		pServer->m_Fifo.Init(pServer->Console(), pServer->Config()->m_SvInputFifo, CFGFLAG_SERVER);
		pServer->Antibot()->Init();
		GameServer()->OnInit(nullptr);
		pServer->ReadAnnouncementsFile();
		pServer->InitMaplist();

		pServer->m_pConsole->StoreCommands(false);
		pServer->m_pRegister->OnConfigChange();
	};

	~GameWorld()
	{
		m_pServer->m_pRegister->OnShutdown();
		m_pServer->m_Econ.Shutdown();
		m_pServer->m_Fifo.Shutdown();
		m_pServer->Engine()->ShutdownJobs();
		m_pServer->GameServer()->OnShutdown(nullptr);
		m_pServer->m_pMap->Unload();
		m_pServer->DbPool()->OnShutdown();
		m_pServer->m_NetServer.Close();
		m_pServerLogger->OnServerDeletion();
		delete m_pKernel;
	};
};

TEST_F(GameWorld, ClosestCharacter)
{
	CNetObj_PlayerInput Input = {};
	CCharacter *pChr1 = new(0) CCharacter(&GameServer()->m_World, Input);
	pChr1->m_Pos = vec2(0, 0);
	GameServer()->m_World.InsertEntity(pChr1);

	CCharacter *pChr2 = new(1) CCharacter(&GameServer()->m_World, Input);
	pChr2->m_Pos = vec2(10, 10);
	GameServer()->m_World.InsertEntity(pChr2);

	CCharacter *pClosest = GameServer()->m_World.ClosestCharacter(vec2(1, 1), 20, nullptr);
	EXPECT_EQ(pClosest, pChr1);
}
