#include "test.h"

#include <base/str.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <gtest/gtest.h>

#include <memory>

static void RunConfigManagerInit(IStorage *pStorage, IConfigManager::EInitializationType InitializationType)
{
	std::unique_ptr<IKernel> pKernel(IKernel::Create());
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	std::unique_ptr<IConfigManager> pConfigManager(CreateConfigManager());
	pKernel->RegisterInterface(pStorage, false);
	pKernel->RegisterInterface(pConsole.get(), false);
	pKernel->RegisterInterface(pConfigManager.get(), false);
	pConsole->Init();
	pConfigManager->Init(InitializationType);
}

// Reproduces what the integrated Emscripten server does while the client is
// already running: it initializes a second CConfigManager in the same process.
// Both config managers point at the single global g_Config, so the second
// initialization must only set the defaults of the variables exclusive to the
// server. Setting all defaults would reset the running client's entire
// configuration (and the client would persist the wipe on exit); setting none
// would leak the server settings of one local-server run into the next.
TEST(Config, SecondConfigManagerForIntegratedServerOnlyResetsServerValues)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_TRUE(pStorage);

	// The first config manager in the process sets the defaults, like the
	// client does on startup.
	RunConfigManagerInit(pStorage.get(), IConfigManager::EInitializationType::STANDALONE);

	// The running client's settings, including a variable shared between
	// client and server (password).
	str_copy(g_Config.m_PlayerName, "MyName");
	str_copy(g_Config.m_ClPlayerSkin, "pinky");
	g_Config.m_SndVolume = 42;
	str_copy(g_Config.m_Password, "hunter2");

	// What a previous local-server run left behind.
	str_copy(g_Config.m_SvMap, "Tutorial");
	str_copy(g_Config.m_SvMotd, "Tutorial MOTD");
	g_Config.m_SvHit = 0;
	g_Config.m_EcPort = 1234;

	RunConfigManagerInit(pStorage.get(), IConfigManager::EInitializationType::INTEGRATED_SERVER);

	// The client's variables and shared variables keep their values.
	EXPECT_STREQ(g_Config.m_PlayerName, "MyName");
	EXPECT_STREQ(g_Config.m_ClPlayerSkin, "pinky");
	EXPECT_EQ(g_Config.m_SndVolume, 42);
	EXPECT_STREQ(g_Config.m_Password, "hunter2");

	// The variables exclusive to the server are reset to their defaults.
	EXPECT_STREQ(g_Config.m_SvMap, DefaultConfig::SvMap);
	EXPECT_STREQ(g_Config.m_SvMotd, DefaultConfig::SvMotd);
	EXPECT_EQ(g_Config.m_SvHit, DefaultConfig::SvHit);
	EXPECT_EQ(g_Config.m_EcPort, DefaultConfig::EcPort);

	// Initializing with all defaults resets the remaining values, which also
	// restores the defaults for the other tests in this process.
	RunConfigManagerInit(pStorage.get(), IConfigManager::EInitializationType::STANDALONE);
	EXPECT_STREQ(g_Config.m_PlayerName, DefaultConfig::PlayerName);
	EXPECT_STREQ(g_Config.m_ClPlayerSkin, DefaultConfig::ClPlayerSkin);
	EXPECT_EQ(g_Config.m_SndVolume, DefaultConfig::SndVolume);
	EXPECT_STREQ(g_Config.m_Password, DefaultConfig::Password);
}

// While the integrated Emscripten server is running it owns the game config
// variables of the shared g_Config, so the client marks them read-only while
// loading the settings of a map (a demo's or another server's map). Map
// settings are executed with CLIENT_ID_GAME, which widens the console's flag
// mask to reach the game variables, so this only works because the read-only
// check is independent of the flag mask.
TEST(Config, ReadOnlyGameSettingsCannotBeChangedByMapSettings)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_TRUE(pStorage);

	std::unique_ptr<IKernel> pKernel(IKernel::Create());
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_CLIENT);
	std::unique_ptr<IConfigManager> pConfigManager(CreateConfigManager());
	pKernel->RegisterInterface(pStorage.get(), false);
	pKernel->RegisterInterface(pConsole.get(), false);
	pKernel->RegisterInterface(pConfigManager.get(), false);
	pConsole->Init();
	pConfigManager->Init(IConfigManager::EInitializationType::STANDALONE);

	// The value that the running server loaded from its own map.
	g_Config.m_SvHit = 1;

	pConfigManager->SetGameSettingsReadOnly(true);
	pConsole->ExecuteLine("sv_hit 0", IConsole::CLIENT_ID_GAME);
	EXPECT_EQ(g_Config.m_SvHit, 1);

	pConfigManager->SetGameSettingsReadOnly(false);
	pConsole->ExecuteLine("sv_hit 0", IConsole::CLIENT_ID_GAME);
	EXPECT_EQ(g_Config.m_SvHit, 0);

	g_Config.m_SvHit = DefaultConfig::SvHit;
}
