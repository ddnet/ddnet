#include "local_server.h"

#include <base/fs.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/process.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/serverbrowser.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <cstdlib>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

// File that the server writes its port to, unique per client process
static const char *PortFile(char *pBuf, unsigned BufSize)
{
	str_format(pBuf, BufSize, "local_server_port.%d.txt", process_id());
	return pBuf;
}

void CLocalServer::Reset()
{
	mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
	m_Port = 0;
	m_WaitingForPort = false;
	m_ConnectRequested = false;

	char aPortFile[IO_MAX_PATH_LENGTH];
	Storage()->RemoveFile(PortFile(aPortFile, sizeof(aPortFile)), IStorage::TYPE_SAVE);
}

void CLocalServer::WaitForPort()
{
	m_WaitingForPort = true;
	m_StartTime = time_get_nanoseconds();
	m_NextPortCheck = m_StartTime;
}

void CLocalServer::OnServerStarted()
{
	WaitForPort();
	GameClient()->m_Menus.ForceRefreshLanPage();
}

bool CLocalServer::RunServer(const std::vector<const char *> &vpArguments)
{
	Reset();

	char aPortFile[IO_MAX_PATH_LENGTH];
	char aPortFileCommand[IO_MAX_PATH_LENGTH + 16];
	str_format(aPortFileCommand, sizeof(aPortFileCommand), "sv_port_file %s", PortFile(aPortFile, sizeof(aPortFile)));

	secure_random_password(m_aRconPassword, sizeof(m_aRconPassword), 16);
	char aAuthCommand[64 + sizeof(m_aRconPassword)];
	str_format(aAuthCommand, sizeof(aAuthCommand), "auth_add %s admin %s", DEFAULT_SAVED_RCON_USER, m_aRconPassword);

	std::vector<const char *> vpArgumentsWithAuth = vpArguments;
	vpArgumentsWithAuth.push_back(aPortFileCommand);
	vpArgumentsWithAuth.push_back(aAuthCommand);

#if defined(CONF_PLATFORM_ANDROID)
	if(StartAndroidServer(vpArgumentsWithAuth.data(), vpArgumentsWithAuth.size()))
	{
		OnServerStarted();
		return true;
	}
	else
	{
		Client()->AddWarning(SWarning(Localize("Server could not be started. Make sure to grant the notification permission in the app settings so the server can run in the background.")));
		Reset();
		return false;
	}
#else
	char aBuf[IO_MAX_PATH_LENGTH];
	Storage()->GetBinaryPath(PLAT_SERVER_EXEC, aBuf, sizeof(aBuf));
#if defined(CONF_PLATFORM_MACOS)
	if(!fs_is_file(aBuf) && fs_parent_dir(aBuf) == 0)
	{
		str_append(aBuf, "/../../../DDNet-Server.app/Contents/MacOS/");
		str_append(aBuf, PLAT_SERVER_EXEC);
	}
#endif
	// No / in binary path means to search in $PATH, so it is expected that the file can't be opened. Just try executing anyway.
	if(str_find(aBuf, "/") == nullptr || fs_is_file(aBuf))
	{
		m_Process = process_execute(aBuf, EShellExecuteWindowState::BACKGROUND, vpArgumentsWithAuth.data(), vpArgumentsWithAuth.size());
		if(m_Process != INVALID_PROCESS)
		{
			OnServerStarted();
			return true;
		}
		else
		{
			Client()->AddWarning(SWarning(Localize("Server could not be started")));
			Reset();
			return false;
		}
	}
	else
	{
		Client()->AddWarning(SWarning(Localize("Server executable not found, can't run server")));
		Reset();
		return false;
	}
#endif
}

void CLocalServer::KillServer()
{
#if defined(CONF_PLATFORM_ANDROID)
	ExecuteAndroidServerCommand("shutdown");
	GameClient()->m_Menus.ForceRefreshLanPage();
#else
	if(m_Process != INVALID_PROCESS && process_kill(m_Process))
	{
		m_Process = INVALID_PROCESS;
		GameClient()->m_Menus.ForceRefreshLanPage();
	}
#endif
	Reset();
}

bool CLocalServer::IsServerRunning()
{
#if defined(CONF_PLATFORM_ANDROID)
	return IsAndroidServerRunning();
#else
	if(m_Process != INVALID_PROCESS && !process_is_alive(m_Process))
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

void CLocalServer::Connect()
{
	// The server reports its port asynchronously, so connect as soon as it is known
	m_ConnectRequested = true;
	if(m_Port == 0 && !m_WaitingForPort)
	{
		WaitForPort();
	}
}

void CLocalServer::CancelConnect()
{
	m_ConnectRequested = false;
}

void CLocalServer::Update()
{
	if(m_WaitingForPort && time_get_nanoseconds() >= m_NextPortCheck)
	{
		m_NextPortCheck = time_get_nanoseconds() + std::chrono::milliseconds(100);
		char aPortFile[IO_MAX_PATH_LENGTH];
		char *pPort = Storage()->ReadFileStr(PortFile(aPortFile, sizeof(aPortFile)), IStorage::TYPE_SAVE);
		if(pPort != nullptr)
		{
			const int Port = str_toint(pPort);
			free(pPort);
			Storage()->RemoveFile(aPortFile, IStorage::TYPE_SAVE);
			// Ignore a file without a valid port, the fallback below decides then
			if(Port > 0 && Port <= 65535)
			{
				m_Port = Port;
				m_WaitingForPort = false;
			}
		}

		if(m_WaitingForPort)
		{
			const std::chrono::nanoseconds Waiting = time_get_nanoseconds() - m_StartTime;
			if(Waiting > std::chrono::seconds(5) && !IsServerRunning())
			{
				// The Android server service only starts running asynchronously, so give
				// the server time to start before considering it failed
				m_WaitingForPort = false;
				if(m_ConnectRequested)
				{
					m_ConnectRequested = false;
					Client()->AddWarning(SWarning(Localize("Server could not be started")));
				}
			}
			else if(Waiting > std::chrono::seconds(5))
			{
				// The server is running but never reported its port, could lack
				// permissions to write port file.
				m_Port = IServerBrowser::LAN_PORT_BEGIN;
				m_WaitingForPort = false;
				mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
			}
		}
	}

	if(m_ConnectRequested)
	{
		const IClient::EClientState State = Client()->State();
		if(State != IClient::STATE_OFFLINE && State != IClient::STATE_ONLINE)
		{
			// A connection that the user started in the meantime supersedes the request
			m_ConnectRequested = false;
		}
		else if(m_Port != 0)
		{
			m_ConnectRequested = false;
			char aAddress[32];
			str_format(aAddress, sizeof(aAddress), "localhost:%d", m_Port);
			GameClient()->m_Menus.Connect(aAddress);
		}
	}
}
