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

		int m_State = 0;
		int64_t m_TimeConnected = 0;
		int m_AuthTries = 0;
	};
	CClient m_aClients[NET_MAX_CONSOLE_CLIENTS];

	CConfig *m_pConfig = nullptr;
	IConsole *m_pConsole = nullptr;
	CNetConsole m_NetConsole;

	bool m_Ready = false;
	int m_PrintCBIndex = 0;
	int m_UserClientID = 0;

	static void SendLineCB(const char *pLine, void *pUserData, ColorRGBA PrintColor = {1, 1, 1, 1});
	static void ConLogout(IConsole::IResult *pResult, void *pUserData);

	static int NewClientCallback(int ClientID, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);

public:
	CEcon();
	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, CNetBan *pNetBan);
	void Update();
	void Send(int ClientID, const char *pLine);
	void Shutdown();
};

#endif
