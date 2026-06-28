#ifndef ENGINE_SHARED_SSH_SERVER_H
#define ENGINE_SHARED_SSH_SERVER_H

#include <engine/shared/config.h>
#include <engine/shared/network.h>

class CSshServer
{
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	CNetConsole m_NetConsole;

public:
	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, CNetBan *pNetBan);
	void Update();
	void Shutdown();
};

#endif
