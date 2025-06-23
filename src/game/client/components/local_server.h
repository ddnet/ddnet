#ifndef GAME_CLIENT_COMPONENTS_LOCAL_SERVER_H
#define GAME_CLIENT_COMPONENTS_LOCAL_SERVER_H

#include <base/types.h>

#include <game/client/component.h>

class CLocalServer : public CComponentInterfaces
{
public:
	void RunServer(const std::vector<const char *> &vpArguments);
	void KillServer();
	bool IsServerRunning();
	void RconAuthIfPossible();

private:
	char m_aRconPassword[sizeof(g_Config.m_SvRconPassword)] = "";

#if !defined(CONF_PLATFORM_ANDROID)
	PROCESS m_Process = INVALID_PROCESS;
#endif
};

#endif
