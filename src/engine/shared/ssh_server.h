#ifndef ENGINE_SHARED_SSH_SERVER_H
#define ENGINE_SHARED_SSH_SERVER_H

#if defined(CONF_SSH)

#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/storage.h>

#include <libssh/libssh.h>
#include <libssh/server.h>

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

	// HOLY STATE HANDLING???
	// refactor this?
	bool m_Authenticated = false;
	bool m_ShellReady = false;
	bool m_ShowBanner = true;

	ssh_session m_Session;
	ssh_channel m_Channel = nullptr;

	int64_t m_JoinTime = 0;
	char m_aInput[2048] = "";
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

	bool TryAuthenticateClient(CSshClient *pClient);
	bool TryOpenSessionChannel(CSshClient *pClient);
	bool TryAcceptShell(CSshClient *pClient);

	std::optional<int> FindFreeSlot();

	void OnClientConnect(int ClientId, ssh_session Session);
	void OnClientDisconnect(int ClientId, const char *pReason = "");

	void AcceptNewConnections();
	void HandleInput(CSshClient *pClient);

public:
	CSshClient *m_apClients[MAX_SSH_CLIENTS] = {};

	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage, CNetBan *pNetBan);
	void Update();
	void Shutdown();
	bool GotActiveConnections();
};

#endif

#endif
