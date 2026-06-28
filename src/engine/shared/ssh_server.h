#ifndef ENGINE_SHARED_SSH_SERVER_H
#define ENGINE_SHARED_SSH_SERVER_H

#include <engine/shared/config.h>
#include <engine/shared/network.h>

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT "2222"
#define HOSTKEY_FILE "ssh_host_rsa_key"
#define USERNAME "demo"
#define PASSWORD "secret"

class CSshServer
{
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	CNetConsole m_NetConsole;

	ssh_bind m_Bind;

public:
	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, CNetBan *pNetBan);
	void Update();
	void Shutdown();
};

#endif
