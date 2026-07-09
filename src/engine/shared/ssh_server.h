#ifndef ENGINE_SHARED_SSH_SERVER_H
#define ENGINE_SHARED_SSH_SERVER_H

#if defined(CONF_SSH)

#include <base/logger.h>
#include <base/types.h>
#include <base/vmath.h>

#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/ringbuffer.h>
#include <engine/storage.h>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include <optional>

static constexpr int MAX_SSH_CLIENTS = 16;

class CSshServer;

// Not thread-safe!
class CSshLogger : public ILogger
{
	CSshServer *m_pSshServer;
	int m_ClientId;
	ILogger *m_pOuterLogger;

public:
	CSshLogger(CSshServer *pSshServer, int ClientId, ILogger *pOuterLogger) :
		m_pSshServer(pSshServer),
		m_ClientId(ClientId),
		m_pOuterLogger(pOuterLogger)
	{
	}
	void Log(const CLogMessage *pMessage) override;
};

class CSshClient
{
public:
	const IConsole *Console() const;
	IConsole *Console();

	CSshClient(int ClientId, ssh_session Session)
	{
		m_ClientId = ClientId;
		m_Session = Session;
	}

	int m_ClientId;
	NETADDR m_Addr;

	// HOLY STATE HANDLING???
	// refactor this?
	bool m_Authenticated = false;
	bool m_ShellReady = false;
	bool m_Dropped = false;

	ssh_session m_Session;
	ssh_channel m_Channel = nullptr;

	int64_t m_JoinTime = 0;

	char m_aInput[2048] = "";

	// position of the cursor in the m_aInput buffer
	// sent to the client and used as insert offset when typing
	ivec2 m_CursorPos = ivec2(0, 0);

	// overwrites the entire m_aInput buffer
	// and sends the new line to the client over the ssh channel
	void SetInput(const char *pInput);

	// clears the current line and places an empty prompt
	void ClearPrompt() const;

	// sends new line and prompt
	void NewPrompt() const;

	void SendCursorPos(ivec2 Pos) const;

	void ResetCompletion();
	void CompleteCommands(bool IsReverse);

	char m_aCompletionBuffer[2048] = "";
	int m_CompletionIndex = -1;
	int m_CompletionEnumerationCount = -1;

	static void CompletionCallback(int Index, const char *pCmd, void *pUser);

	class CTerminal
	{
	public:
		int m_Width = 10;
		int m_Height = 10;
	};
	CTerminal m_Term;

	CStaticRingBuffer<char, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	char *m_pHistoryEntry = nullptr;

	// We need to manage the memory for this struct
	// because libssh does not copy it
	struct ssh_server_callbacks_struct m_ServerCallback = {};
	struct ssh_channel_callbacks_struct m_ChannelCallback = {};

	// TODO: I wonder if just passing *this* would be enough
	//       i think it works for now but then later we might need the entire server in scope
	//       to ban on too many login attempts or something like that
	class CCallbackCtx
	{
	public:
		CSshClient *m_pClient = nullptr;
		CSshServer *m_pServer = nullptr;
	};
	CCallbackCtx m_CallbackCtx;
};

class CSshServer
{
	CConfig *m_pConfig = nullptr;
	IConsole *m_pConsole = nullptr;
	IStorage *m_pStorage = nullptr;

	const IStorage *Storage() const { return m_pStorage; }
	IStorage *Storage() { return m_pStorage; }

	ssh_bind m_Bind = nullptr;

	char m_aError[512] = "";

	void GenerateHostKeyIfMissing();
	void ProcessMessage(CSshClient *pClient);

	std::optional<int> FindFreeSlot();

	// libssh callbacks
	static int AuthPasswordCallback(ssh_session Session, const char *pUsername, const char *pPassword, void *pUserData);
	static int AuthPubkeyCallback(ssh_session Session, const char *pUsername, struct ssh_key_struct *pClientPubKey, char SignatureState, void *pUserData);
	static ssh_channel ChannelOpenRequestSessionCallback(ssh_session Session, void *pUserData);
	static int ChannelPtyRequestCallback(ssh_session Session, ssh_channel Channel, const char *pTerm, int Width, int Height, int PxWidth, int PwHeight, void *pUserData);
	static int ChannelPtyWindowChangeCallback(ssh_session Session, ssh_channel Channel, int Width, int Height, int PxWidth, int PwHeight, void *pUserData);
	static int ChannelExecRequestCallback(ssh_session Session, ssh_channel Channel, const char *pCommand, void *pUserData);
	static int ChannelShellRequestCallback(ssh_session Session, ssh_channel Channel, void *pUserData);

	void OnClientConnect(int ClientId, ssh_session Session);
	void OnClientDisconnect(int ClientId, const char *pReason = "");

	void AcceptNewConnections();
	void ListConnections();
	void ExecuteRconLine(CSshClient *pClient, const char *pLine);
	void HandleInput(CSshClient *pClient);

public:
	CSshClient *m_apClients[MAX_SSH_CLIENTS] = {};

	IConsole *Console() { return m_pConsole; }

	void Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage);
	void Update();
	void Shutdown();
	bool GotActiveConnections();
};

#endif

#endif
