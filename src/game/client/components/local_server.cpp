#include <game/client/gameclient.h>

#include <game/localization.h>

#include "local_server.h"

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

static constexpr const char *DEFAULT_SAVED_RCON_USER = "local-server";

void CLocalServer::RunServer(const std::vector<const char *> &vpArguments)
{
	secure_random_password(m_aRconPassword, sizeof(m_aRconPassword), 16);
	char aAuthCommand[64 + sizeof(m_aRconPassword)];
	str_format(aAuthCommand, sizeof(aAuthCommand), "auth_add %s admin %s", DEFAULT_SAVED_RCON_USER, m_aRconPassword);

	std::vector<const char *> vpArgumentsWithAuth = vpArguments;
	vpArgumentsWithAuth.push_back(aAuthCommand);

#if defined(CONF_PLATFORM_ANDROID)
	if(StartAndroidServer(vpArgumentsWithAuth.data(), vpArgumentsWithAuth.size()))
	{
		GameClient()->m_Menus.ForceRefreshLanPage();
	}
	else
	{
		Client()->AddWarning(SWarning(Localize("Server could not be started. Make sure to grant the notification permission in the app settings so the server can run in the background.")));
		mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
	}
#else
	char aBuf[IO_MAX_PATH_LENGTH];
	Storage()->GetBinaryPath(PLAT_SERVER_EXEC, aBuf, sizeof(aBuf));
	// No / in binary path means to search in $PATH, so it is expected that the file can't be opened. Just try executing anyway.
	if(str_find(aBuf, "/") == nullptr || fs_is_file(aBuf))
	{
		m_Process = shell_execute(aBuf, EShellExecuteWindowState::BACKGROUND, vpArgumentsWithAuth.data(), vpArgumentsWithAuth.size());
		if(m_Process != INVALID_PROCESS)
		{
			GameClient()->m_Menus.ForceRefreshLanPage();
		}
		else
		{
			Client()->AddWarning(SWarning(Localize("Server could not be started")));
			mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
		}
	}
	else
	{
		Client()->AddWarning(SWarning(Localize("Server executable not found, can't run server")));
		mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
	}
#endif
}

void CLocalServer::KillServer()
{
#if defined(CONF_PLATFORM_ANDROID)
	ExecuteAndroidServerCommand("shutdown");
	GameClient()->m_Menus.ForceRefreshLanPage();
#else
	if(m_Process != INVALID_PROCESS && kill_process(m_Process))
	{
		m_Process = INVALID_PROCESS;
		GameClient()->m_Menus.ForceRefreshLanPage();
	}
#endif
	mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
}

bool CLocalServer::IsServerRunning()
{
#if defined(CONF_PLATFORM_ANDROID)
	return IsAndroidServerRunning();
#else
	if(m_Process != INVALID_PROCESS && !is_process_alive(m_Process))
	{
		KillServer();
	}
	return m_Process != INVALID_PROCESS;
#endif
}

void CLocalServer::RconAuthIfPossible()
{
	if(!IsServerRunning() ||
		m_aRconPassword[0] == '\0' ||
		!net_addr_is_local(&Client()->ServerAddress()))
	{
		return;
	}
	Client()->RconAuth(DEFAULT_SAVED_RCON_USER, m_aRconPassword, g_Config.m_ClDummy);
}
