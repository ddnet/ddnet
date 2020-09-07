#ifndef GAME_CLIENT_ECON_CLIENT_H
#define GAME_CLIENT_ECON_CLIENT_H

class CGameConsole;

#include <engine/shared/network.h>

class CEconClient
{
	int m_State;
	CGameConsole *m_pGameConsole;
	NETSOCKET m_Socket;
	CConsoleNetConnection m_Connection;

public:
	enum
	{
		STATE_OFFLINE=0,
		STATE_CONNECTED,
		STATE_AUTHED,
	};

	void Init(CGameConsole *pGameConsole);
	void Connect(const char *pAddress, const char *pPassword);
	void Disconnect(const char *pReason);
	void Send(const char *pLine);
	void Update();
	int State() const { return m_State; };
};

#endif
