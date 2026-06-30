#ifndef ENGINE_SHARED_SSH_SERVER_H
#define ENGINE_SHARED_SSH_SERVER_H

#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/storage.h>

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
	CConfig *m_pConfig = nullptr;
	IConsole *m_pConsole = nullptr;
	IStorage *m_pStorage = nullptr;
	CNetConsole m_NetConsole;

	const IStorage *Storage() const { return m_pStorage; }
	IStorage *Storage() { return m_pStorage; }

	ssh_bind m_Bind;

	char m_aError[512] = "";

	void GenerateHostKeyIfMissing();

public:
	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage, CNetBan *pNetBan);
	void Update();
	void Shutdown();
};

#endif
