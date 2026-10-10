#ifndef GAME_CLIENT_COMPONENTS_LOCAL_SERVER_H
#define GAME_CLIENT_COMPONENTS_LOCAL_SERVER_H

#include <base/types.h>

#include <engine/shared/config.h>

#include <game/client/component.h>

#include <chrono>

class CLocalServer : public CComponentInterfaces
{
public:
	bool RunServer(const std::vector<const char *> &vpArguments);
	void KillServer();
	bool IsServerRunning();
	bool IsStarting() const { return m_WaitingForPort; }
	void Update();
	void RconAuthIfPossible();
	void Connect();
	void CancelConnect();

private:
	void Reset();
	void WaitForPort();
	void OnServerStarted();

	char m_aRconPassword[sizeof(g_Config.m_SvRconPassword)] = "";
	// Port that the server reported after binding its socket, 0 until then
	int m_Port = 0;
	bool m_WaitingForPort = false;
	std::chrono::nanoseconds m_StartTime{0};
	std::chrono::nanoseconds m_NextPortCheck{0};
	bool m_ConnectRequested = false;

#if !defined(CONF_PLATFORM_ANDROID)
	PROCESS m_Process = INVALID_PROCESS;
#endif
};

#endif
