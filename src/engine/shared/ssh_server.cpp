#if defined(CONF_SSH)

#include "ssh_server.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/storage.h>

#include <fcntl.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

// TODO: offer pty in addition to shell? I feel like it is more powerfull for stuff like autocomplete and shit

// TODO: ban ip after too many failed login attempts, do we also need some delay in the password check to protect against bruteforcing?

// TODO: log failed auth somewhere? ssh is a more popular attack target than teeworlds econ
//       we might need extra protection by showing the admin how many incoming attacks are there

// TODO: should there be some "w" or "who" command like in linux systems
//       to see other ssh connections because they do not show up in "status"

// TODO: should be a config
#define PORT "2222"

// TODO: the default file location in the current dir is not ideal
//       this should be in the storage save location
//       or maybe even use the system wide location that also the regular host
//       ssh server uses so we do not need to generate the key
#define HOSTKEY_FILE "ssh_host_rsa_key"

// TODO: have "root" user by default use the sv_rcon_password pass
//       or maybe it should be "default_admin" thats how the status command calls it
//       in addition to that register all keys added by the auth manager as valid credentials
//       for moderators and helpers
#define USERNAME "demo"
#define PASSWORD "secret"

// TODO: there should also be ssh pub key login

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

void CSshLogger::Log(const CLogMessage *pMessage)
{
	CSshClient *pClient = m_pSshServer->m_apClients[m_ClientId];
	if(!pClient)
	{
		// TODO: assert instead?
		//       can this happen if we disconnect during a running operation?
		m_pOuterLogger->Log(pMessage);
		return;
	}

	ssh_channel_write(pClient->m_Channel, "\r\n", 2);
	ssh_channel_write(pClient->m_Channel, pMessage->Message(), str_length(pMessage->Message()));

	// just mirror everything to regular log because the hiding is stupid
	// https://github.com/ddnet/ddnet/issues/11095
	m_pOuterLogger->Log(pMessage);
}

bool CSshServer::TryAuthenticateClient(CSshClient *pClient)
{
	ssh_session Session = pClient->m_Session;
	ssh_message Message = ssh_message_get(Session);

	// log_info("ssh", "trying to authenticate cid %d...", pClient->m_ClientId);

	if(Message == nullptr)
		return true;

	if(ssh_message_type(Message) == SSH_REQUEST_AUTH &&
		ssh_message_subtype(Message) == SSH_AUTH_METHOD_PASSWORD)
	{
		const char *pUser = ssh_message_auth_user(Message);
		const char *pPass = ssh_message_auth_password(Message);

		if(pUser && pPass &&
			str_comp(pUser, USERNAME) == 0 &&
			str_comp(pPass, PASSWORD) == 0)
		{
			log_info("ssh", "deprecated auth success");
			ssh_message_auth_reply_success(Message, 0);
			pClient->m_Authenticated = true;
		}
		else
		{
			ssh_message_auth_set_methods(Message, SSH_AUTH_METHOD_PASSWORD);
			ssh_message_reply_default(Message);
		}
	}
	else
	{
		ssh_message_auth_set_methods(Message, SSH_AUTH_METHOD_PASSWORD);
		ssh_message_reply_default(Message);
	}

	ssh_message_free(Message);
	return true;
}

bool CSshServer::TryOpenSessionChannel(CSshClient *pClient)
{
	if(pClient->m_Channel)
		return true;

	ssh_message Message = ssh_message_get(pClient->m_Session);
	if(Message == nullptr)
		return true;

	if(ssh_message_type(Message) == SSH_REQUEST_CHANNEL_OPEN &&
		ssh_message_subtype(Message) == SSH_CHANNEL_SESSION)
	{
		pClient->m_Channel = ssh_message_channel_request_open_reply_accept(Message);
		ssh_message_free(Message);
		return true;
	}

	ssh_message_reply_default(Message);
	ssh_message_free(Message);

	return true;
}

void CSshServer::ProcessMessage(CSshClient *pClient)
{
	// we aren't interested in any specific message yet
	// all of the messages are handled in callbacks
	// but if we do not call ssh_message_get() things get stuck
	ssh_session Session = pClient->m_Session;
	ssh_message Message = ssh_message_get(Session);
	if(Message == nullptr)
		return;

	// WARNING: if you check messages here make sure to check pClient->m_Authenticated first if needed

	ssh_message_free(Message);
}

void CSshServer::HandleInput(CSshClient *pClient)
{
	ssh_channel Channel = pClient->m_Channel;
	char aBuf[256] = {0};

	int n = ssh_channel_read_nonblocking(Channel, aBuf, sizeof(aBuf), 0);
	if(n == SSH_EOF || n == SSH_ERROR)
	{
		dbg_assert_failed("TODO: handle this ssh error");
		return;
	}
	if(n == SSH_AGAIN || n == 0)
	{
		return;
	}

	int k = str_length(pClient->m_aInput);
	if(k + n > (int)sizeof(pClient->m_aInput) - 10)
	{
		// TODO: better error handling xd
		ssh_channel_write(Channel, "INPUT FULL LOL ", 15);
		return;
	}

	// TODO: improve this shell!
	// TODO: ctrl+r history support
	// TODO: movement with arrow keys
	// TODO: word jumping and word deletion
	// TODO: autocomplete
	// TODO: can we use the readline library here somehow?

	for(int i = 0; i < n; i++)
	{
		char Byte = aBuf[i];
		if(Byte == 13)
		{
			const char *pCmd = pClient->m_aInput;
			log_info("ssh", "cid=%d cmd='%s'", pClient->m_ClientId, pCmd);

			if(!str_comp(pCmd, "logout") || !str_comp(pCmd, "exit"))
			{
				OnClientDisconnect(pClient->m_ClientId, "logout");
				return;
			}

			{
				CSshLogger Logger(this, pClient->m_ClientId, log_get_scope_logger());
				CLogScope Scope(&Logger);
				Console()->ExecuteLine(pCmd, IConsole::CLIENT_ID_UNSPECIFIED, true);
			}

			pClient->m_aInput[0] = '\0';
			ssh_channel_write(Channel, "\r\n> ", 4);
			return;
		}
		else if(Byte == 21) // ctrl+u
		{
			// ideally this would not be the same as ctrl+c
			// and just clear the current prompt instead of
			// opening a new one
			pClient->m_aInput[0] = '\0';
			ssh_channel_write(Channel, "\r\n> ", 4);
			continue;
		}
		else if(Byte == 3) // ctrl+c
		{
			if(pClient->m_aInput[0] == '\0')
			{
				const char *pMsg = "\r\nUse ctrl+d or 'exit' to quit";
				ssh_channel_write(Channel, pMsg, str_length(pMsg));
			}

			pClient->m_aInput[0] = '\0';
			ssh_channel_write(Channel, "\r\n> ", 4);
			continue;
		}
		else if(Byte == 4) // ctrl+d
		{
			// silently ignore ctrl+d if there is still input
			if(pClient->m_aInput[0])
				continue;

			OnClientDisconnect(pClient->m_ClientId, "logout");
			return;
		}
		else if(Byte == 127 || Byte == '\b')
		{
			int LastChr = str_length(pClient->m_aInput);
			LastChr = std::max(0, LastChr - 1);
			pClient->m_aInput[LastChr] = '\0';
			ssh_channel_write(pClient->m_Channel, "\b \b", 3);
			continue;
		}
		pClient->m_aInput[k] = Byte;
		pClient->m_aInput[k + 1] = '\0';
		k++;
		ssh_channel_write(Channel, aBuf + i, 1);
	}

	// log_info("ssh", "got msg '%s' id=%d", aBuf, aBuf[0]);
	// log_info("ssh", " id=%d", aBuf[0]);
	// log_info("ssh", " new input '%s'", pClient->m_aInput);

	// aBuf[n] = '\0';
	// ssh_channel_write(Channel, "echo: ", 6);
	// ssh_channel_write(Channel, aBuf, n);
}

void CSshServer::GenerateHostKeyIfMissing()
{
#ifdef CONF_PLATFORM_LINUX
	// TODO: make this cross platform and use openssh C++ code
	int Ret = system("bash -c \"[[ -f ssh_host_rsa_key ]] || ssh-keygen -t rsa -b 4096 -f ssh_host_rsa_key -N ''\"");
	if(Ret != 0)
	{
		str_copy(m_aError, "host key generation failed");
		log_error("ssh", "failed to generate host key");
	}
#endif
}

void CSshServer::Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage, CNetBan *pNetBan)
{
	m_pConfig = pConfig;
	m_pConsole = pConsole;
	m_pStorage = pStorage;

	log_info("ssh", "libssh %s", ssh_version(0));

	m_Bind = ssh_bind_new();
	if(m_Bind == nullptr)
	{
		str_copy(m_aError, "failed to bind");
		log_error("ssh", "failed to create ssh_bind");
		return;
	}

	GenerateHostKeyIfMissing();

	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0");
	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_BINDPORT_STR, PORT);
	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_RSAKEY, HOSTKEY_FILE);

	int Ok = ssh_bind_listen(m_Bind);
	if(Ok < 0)
	{
		str_copy(m_aError, "listen error");
		log_error("ssh", "listen error: %s", ssh_get_error(m_Bind));
		ssh_bind_free(m_Bind);
		return;
	}

	ssh_bind_set_blocking(m_Bind, 0);

	int RawFd = ssh_bind_get_fd(m_Bind);
	fcntl(RawFd, F_SETFL, O_NONBLOCK);

	log_info("ssh", "listening on 0.0.0.0:%s", PORT);
	log_info("ssh", "username: %s", USERNAME);
	log_info("ssh", "password: %s", PASSWORD);
}

std::optional<int> CSshServer::FindFreeSlot()
{
	for(int i = 0; i < MAX_SSH_CLIENTS; i++)
		if(m_apClients[i] == nullptr)
			return i;
	return std::nullopt;
}

int CSshServer::AuthPasswordCallback(ssh_session Session, const char *pUsername, const char *pPassword, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	log_info("ssh", "password auth attempt — user='%s'", pUsername);

	if(str_comp(pUsername, USERNAME) == 0 &&
		str_comp(pPassword, PASSWORD) == 0)
	{
		log_info("ssh", "Password auth: SUCCESS");
		pCtx->m_pClient->m_Authenticated = true;
		return SSH_AUTH_SUCCESS;
	}

	log_info("ssh", "Password auth: DENIED");
	return SSH_AUTH_DENIED;
}

ssh_channel CSshServer::ChannelOpenRequestSessionCallback(ssh_session Session, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	log_info("ssh", "Client requesting to open a session channel...");

	// Security check: Ensure the user actually authenticated
	if(!pCtx->m_pClient->m_Authenticated)
	{
		log_info("ssh", "Channel open DENIED: User not authenticated.");
		return nullptr;
	}

	// Create a new channel for this session
	ssh_channel Channel = ssh_channel_new(Session);
	if(Channel == nullptr)
	{
		log_info("ssh", "Failed to create channel.");
		return nullptr;
	}

	// Optional: Store the channel in your client object so you can read/write to it later
	pCtx->m_pClient->m_Channel = Channel;

	pCtx->m_pClient->m_ChannelCallback = {
		.userdata = pCtx,
		.channel_pty_request_function = ChannelPtyRequestCallback,
		.channel_shell_request_function = ChannelShellRequestCallback,
		.channel_pty_window_change_function = ChannelPtyWindowChangeCallback,
	};
	ssh_callbacks_init(&pCtx->m_pClient->m_ChannelCallback);
	ssh_set_channel_callbacks(Channel, &pCtx->m_pClient->m_ChannelCallback);

	log_info("ssh", "Channel open: SUCCESS");
	return Channel;
}

int CSshServer::ChannelPtyRequestCallback(ssh_session Session, ssh_channel Channel, const char *pTerm, int Width, int Height, int PxWidth, int PwHeight, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx =
		static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		log_info("ssh", "PTY request denied: client not authenticated");
		return SSH_ERROR;
	}

	log_warn("ssh", "PTY NOT SUPPORTED");

	// we don't support pty yet, only shell for now
	// smh we still need to return ok here??
	return SSH_OK;
}

int CSshServer::ChannelPtyWindowChangeCallback(ssh_session Session, ssh_channel Channel, int Width, int Height, int PxWidth, int PwHeight, void *pUserData)
{
	log_info("ssh", "pty window change");
	return 0;
}

int CSshServer::ChannelShellRequestCallback(ssh_session Session, ssh_channel Channel, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx =
		static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		log_info("ssh", "Shell request denied: client not authenticated");
		return SSH_ERROR;
	}

	log_info("ssh", "Shell request accepted");

	pCtx->m_pClient->m_Channel = Channel;
	pCtx->m_pClient->m_ShellReady = true;

	const char *pBanner =
		"##################################\r\n"
		"#                                #\r\n"
		"#  welcome to the rcon console!  #\r\n"
		"#                                #\r\n"
		"##################################\r\n";
	ssh_channel_write(Channel, pBanner, str_length(pBanner));
	ssh_channel_write(Channel, "\r\n> ", 4);

	return SSH_OK;
}

void CSshServer::OnClientConnect(int ClientId, ssh_session Session)
{
	dbg_assert(m_apClients[ClientId] == nullptr, "ssh_server connect failed ClientId %d reused", ClientId);

	// TODO: this prints for every connection attempt so attackers without the password can spam the log
	//       and occupy client ids
	//       so ideally there would be a different connection pool just for the before auth state
	log_info("ssh", "client with id %d connected", ClientId);

	CSshClient *pClient = new CSshClient(ClientId, Session);
	pClient->m_JoinTime = time_get();

	pClient->m_CallbackCtx = {
		.m_pClient = pClient,
		.m_pServer = this};

	pClient->m_ServerCallback = {
		.userdata = &pClient->m_CallbackCtx,
		.auth_password_function = AuthPasswordCallback,
		.channel_open_request_session_function = ChannelOpenRequestSessionCallback,
	};
	ssh_callbacks_init(&pClient->m_ServerCallback);
	ssh_set_server_callbacks(Session, &pClient->m_ServerCallback);

	m_apClients[ClientId] = pClient;
}

void CSshServer::OnClientDisconnect(int ClientId, const char *pReason)
{
	CSshClient *pClient = m_apClients[ClientId];
	if(!pClient)
		return;

	log_info("ssh", "client with id %d disconnected", ClientId);

	ssh_channel Channel = pClient->m_Channel;
	if(Channel)
	{
		ssh_channel_free(Channel);
	}
	ssh_session Session = m_apClients[ClientId]->m_Session;
	if(Session)
	{
		ssh_disconnect(Session);
		ssh_free(Session);
	}

	delete m_apClients[ClientId];
	m_apClients[ClientId] = nullptr;
}

void CSshServer::AcceptNewConnections()
{
	ssh_session NewSession = ssh_new();

	// Non-blocking accept
	int Rc = ssh_bind_accept(m_Bind, NewSession);
	if(Rc == SSH_ERROR)
	{
		ssh_free(NewSession);
		return; // No pending connection
	}

	auto Slot = FindFreeSlot();
	if(!Slot.has_value())
	{
		ssh_disconnect(NewSession);
		ssh_free(NewSession);
		return;
	}
	int ClientId = Slot.value();

	ssh_set_blocking(NewSession, 0);

	// Start key exchange (will complete over multiple ticks)
	if(ssh_handle_key_exchange(NewSession) == SSH_AGAIN ||
		ssh_handle_key_exchange(NewSession) == SSH_OK)
	{
		OnClientConnect(ClientId, NewSession);
	}
	else
	{
		ssh_free(NewSession);
	}
}

void CSshServer::Update()
{
	if(m_aError[0])
		return;

	AcceptNewConnections();

	for(CSshClient *pClient : m_apClients)
	{
		if(!pClient)
			continue;

		ProcessMessage(pClient);

		// TODO: should we also timeout sessions without keepalive?
		// TODO: this is not optimal since during the password prompt we can not really send a message
		//       there is no channel yet so the user can still be typing a password after already being disconnected
		//       just to see "bye bye" in the end
		//       but we also can not block a slot for everyone that is in the password state
		//       i wonder how econ does it
		//       can econ be denial of serviced with a bunch of clients in the asking for password state?
		if(!pClient->m_ShellReady)
		{
			int64_t ConnectedSinceSeconds = (time_get() - pClient->m_JoinTime) / time_freq();
			if(ConnectedSinceSeconds > 10)
			{
				log_info("ssh", "cid=%d did not get shell ready fast enough and timed out", pClient->m_ClientId);
				OnClientDisconnect(pClient->m_ClientId, "timeout");
			}
			continue;
		}

		if(!ssh_channel_is_open(pClient->m_Channel) || ssh_channel_is_eof(pClient->m_Channel))
		{
			ssh_channel_send_eof(pClient->m_Channel);
			ssh_channel_close(pClient->m_Channel);
			OnClientDisconnect(pClient->m_ClientId);
		}

		if(!pClient->m_ShellReady)
			continue;
		if(!pClient->m_Channel)
			continue;
		if(!pClient->m_Authenticated)
			continue;

		// TODO: should this be a callback too? Right now it seems to work as is so no urgency.
		HandleInput(pClient);
	}
}

void CSshServer::Shutdown()
{
	if(m_Bind != nullptr && m_aError[0] == '\0')
		ssh_bind_free(m_Bind);
}

bool CSshServer::GotActiveConnections()
{
	return std::ranges::any_of(m_apClients, [](auto *pClient) { return pClient != nullptr; });
}

#endif
