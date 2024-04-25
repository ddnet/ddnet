#ifndef ENGINE_SHARED_ECON_H
#define ENGINE_SHARED_ECON_H

#include "network.h"

#include <engine/console.h>

class CConfig;
class CNetBan;
class ColorRGBA;

class CEcon
{
	enum
	{
		MAX_AUTH_TRIES = 3,
	};

	class CClient
	{
	public:
		enum
		{
			STATE_EMPTY = 0,
			STATE_CONNECTED,
			STATE_AUTHED,
		};

		int m_State;
		int64_t m_TimeConnected;
		int m_AuthTries;
	};
	CClient m_aClients[NET_MAX_CONSOLE_CLIENTS];

	CConfig *m_pConfig;
	IConsole *m_pConsole;
	CNetConsole m_NetConsole;

	bool m_Ready;
	int m_PrintCBIndex;
	int m_UserClientId;

	static void SendLineCB(const char *pLine, void *pUserData, ColorRGBA PrintColor = {1, 1, 1, 1});
	static void ConLogout(IConsole::IResult *pResult, void *pUserData);

	static int NewClientCallback(int ClientId, void *pUser);
	static int DelClientCallback(int ClientId, const char *pReason, void *pUser);

public:
	CEcon();
	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, CNetBan *pNetBan);
	void Update();
	void Send(int ClientId, const char *pLine);
	void Shutdown();
};

#endif
