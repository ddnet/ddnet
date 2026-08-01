#ifndef TEST_GAMEWORLD_TEST_H
#define TEST_GAMEWORLD_TEST_H

#include "test.h"

#include <base/logger.h>
#include <base/types.h>

#include <engine/engine.h>
#include <engine/http.h>
#include <engine/kernel.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/register.h>
#include <engine/server/server.h>
#include <engine/server/server_logger.h>
#include <engine/shared/assertion_logger.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/protocol.h>

#include <game/server/gamecontext.h>
#include <game/version.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>

// Stands up a real `CServer` + `CGameContext` on the `coverage` map, the way a production
// server would. Shared by tests that need to drive actual game logic (`OnTick`, player/character
// creation, ...) rather than just unit-testing an isolated function.
class GameWorld : public ::testing::Test // NOLINT(readability-identifier-naming)
{
public:
	IGameServer *m_pGameServer = nullptr;
	CServer *m_pServer = nullptr;
	std::unique_ptr<IKernel> m_pKernel;
	CTestInfo m_TestInfo;
	std::unique_ptr<IStorage> m_pStorage;

	CGameContext *GameServer() // NOLINT(readability-make-member-function-const)
	{
		return (CGameContext *)m_pGameServer;
	}

	// True once `ShutdownGameServer` has run, so the destructor doesn't run it a second time.
	bool m_GameServerShutdown = false;

	// Finishes any active teehistorian recording and cleanly shuts the game server down.
	// Idempotent (the destructor calls it too, guarded by `m_GameServerShutdown`), so a test can
	// call it explicitly to read back a file the server just wrote (e.g. via `Storage()`) while
	// the fixture is still alive, rather than only being able to do so after the fixture is gone.
	void ShutdownGameServer()
	{
		if(m_GameServerShutdown)
		{
			return;
		}
		m_GameServerShutdown = true;
		m_pGameServer->OnShutdown(nullptr);
	}

	// `EnableTeeHistorian` mirrors `sv_tee_historian`: the real server reads it from the global
	// config at `OnInit` time, so it must be set between `pConfigManager->Init()` (which resets
	// `g_Config` to defaults) and `OnInit` below, not passed to `OnInit` directly.
	explicit GameWorld(bool EnableTeeHistorian = false)
	{
		CServer *pServer = CreateServer();
		m_pServer = pServer;

		m_pKernel = std::unique_ptr<IKernel>(IKernel::Create());
		m_pKernel->RegisterInterface(m_pServer);

		IEngine *pEngine = CreateTestEngine(GAME_NAME);
		m_pKernel->RegisterInterface(pEngine);

		m_TestInfo.m_DeleteTestStorageFilesOnSuccess = true;
		m_pStorage = m_TestInfo.CreateTestStorage();
		EXPECT_NE(m_pStorage, nullptr);
		m_pKernel->RegisterInterface(m_pStorage.get(), false);

		IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON).release();
		m_pKernel->RegisterInterface(pConsole);

		IConfigManager *pConfigManager = CreateConfigManager();
		m_pKernel->RegisterInterface(pConfigManager);

		IEngineHttp *pEngineHttp = CreateEngineHttp();
		m_pKernel->RegisterInterface(pEngineHttp); // IEngineHttp
		m_pKernel->RegisterInterface(static_cast<IHttp *>(pEngineHttp), false);

		IEngineAntibot *pEngineAntibot = CreateEngineAntibot();
		m_pKernel->RegisterInterface(pEngineAntibot);
		m_pKernel->RegisterInterface(static_cast<IAntibot *>(pEngineAntibot), false);

		m_pGameServer = CreateGameServer();
		m_pKernel->RegisterInterface(m_pGameServer);

		pEngine->Init();
		pConsole->Init();
		pConfigManager->Init();

		if(EnableTeeHistorian)
		{
			g_Config.m_SvTeeHistorian = 1;
			// A production `IStorage` gets its `teehistorian/` folder from `CStorage::Init`'s
			// `COMMON_DIRS`; `CreateTempStorage` above (via `CTestInfo::CreateTestStorage`) never
			// calls that, so `CGameContext::OnInit` below would otherwise fail to open the
			// recording file, hit its "teehistorian open error" `SetErrorShutdown` and skip the
			// rest of `OnInit` -- including score handling setup, so the very first
			// `CreatePlayer` afterwards segfaults on a null `Score()`.
			m_pStorage->CreateFolder("teehistorian", IStorage::TYPE_SAVE);
		}

		m_pServer->RegisterCommands();

		EXPECT_NE(m_pServer->LoadMap("coverage"), 0);

		m_pServer->m_RunServer = CServer::RUNNING;

		m_pServer->m_AuthManager.Init();

		{
			int Size = GameServer()->PersistentClientDataSize();
			for(auto &Client : m_pServer->m_aClients)
			{
				Client.m_HasPersistentData = false;
				Client.m_pPersistentData = malloc(Size);
			}
		}
		m_pServer->m_pPersistentData = malloc(GameServer()->PersistentDataSize());
		EXPECT_NE(m_pServer->LoadMap("coverage"), 0);

		EXPECT_TRUE(pEngineHttp->Init(std::chrono::seconds{2})) << "Failed to initialize the HTTP client";

		pServer->m_NetServer.SetCallbacks(
			CServer::NewClientCallback,
			CServer::NewClientNoAuthCallback,
			CServer::ClientRejoinCallback,
			CServer::DelClientCallback, pServer);

		pServer->m_Econ.Init(pServer->Config(), pServer->Console(), &pServer->m_ServerBan);

		pServer->m_Fifo.Init(pServer->Console(), pServer->Config()->m_SvInputFifo, CFGFLAG_SERVER);
		m_pServer->Antibot()->Init();
		GameServer()->OnInit(nullptr);
		pServer->ReadAnnouncementsFile();
		pServer->InitMaplist();
	}

	~GameWorld() override
	{
		m_pServer->m_Econ.Shutdown();
		m_pServer->m_Fifo.Shutdown();
		ShutdownGameServer();
		m_pServer->DbPool()->OnShutdown();
	}
};

#endif // TEST_GAMEWORLD_TEST_H
