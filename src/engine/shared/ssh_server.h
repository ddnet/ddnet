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

#include <optional>

#define PORT "2222"
#define HOSTKEY_FILE "ssh_host_rsa_key"
#define USERNAME "demo"
#define PASSWORD "secret"

static constexpr int MAX_SSH_CLIENTS = 16;

class CSshClient
{
public:
	CSshClient(int ClientId, ssh_session Session)
	{
		m_ClientId = ClientId;
		m_Session = Session;
	}

	int m_ClientId;
	bool m_Authenticated = false;
	bool m_ShellReady = false;
	ssh_session m_Session;
	ssh_channel m_Channel = nullptr;
};

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

	CSshClient *m_apClients[MAX_SSH_CLIENTS] = {};

	std::optional<int> FindFreeSlot();

	void OnClientConnect(int ClientId, ssh_session Session);
	void OnClientDisconnect(int ClientId);

	void AcceptNewConnections();

public:
	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage, CNetBan *pNetBan);
	void Update();
	void Shutdown();
};

#endif
