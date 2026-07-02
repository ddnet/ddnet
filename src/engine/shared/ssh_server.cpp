#include "ssh_server.h"

#include <base/detect.h>

#if defined(CONF_FAMILY_UNIX)

#include <base/dbg.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/storage.h>

#include <fcntl.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#define PORT "2222"
#define HOSTKEY_FILE "ssh_host_rsa_key"
#define USERNAME "demo"
#define PASSWORD "secret"

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

bool CSshServer::TryAcceptShell(CSshClient *pClient)
{
	ssh_session Session = pClient->m_Session;
	ssh_message Message = ssh_message_get(Session);

	// log_info("ssh", "trying to shell ready cid %d...", pClient->m_ClientId);

	if(Message == nullptr)
		return true;

	if(ssh_message_type(Message) == SSH_REQUEST_CHANNEL)
	{
		switch(ssh_message_subtype(Message))
		{
		case SSH_CHANNEL_REQUEST_PTY:
			ssh_message_channel_request_reply_success(Message);
			break;

		case SSH_CHANNEL_REQUEST_SHELL:
			ssh_message_channel_request_reply_success(Message);
			pClient->m_ShellReady = true;
			break;

		default:
			ssh_message_reply_default(Message);
			break;
		}
	}
	else
	{
		ssh_message_reply_default(Message);
	}

	ssh_message_free(Message);
	return true;
}

void CSshServer::HandleInput(CSshClient *pClient)
{
	ssh_channel Channel = pClient->m_Channel;
	char aBuf[256] = {0};

	if(pClient->m_ShowBanner)
	{
		const char *pBanner =
			"##################################\r\n"
			"#                                #\r\n"
			"#  welcome to the rcon console!  #\r\n"
			"#                                #\r\n"
			"##################################\r\n";
		ssh_channel_write(Channel, pBanner, str_length(pBanner));
		ssh_channel_write(Channel, "\r\n> ", 4);
		pClient->m_ShowBanner = false;
	}

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

	log_info("ssh", "Listening on 0.0.0.0:%s", PORT);
	log_info("ssh", "Username: %s", USERNAME);
	log_info("ssh", "Password: %s", PASSWORD);
}

std::optional<int> CSshServer::FindFreeSlot()
{
	for(int i = 0; i < MAX_SSH_CLIENTS; i++)
		if(m_apClients[i] == nullptr)
			return i;
	return std::nullopt;
}

void CSshServer::OnClientConnect(int ClientId, ssh_session Session)
{
	dbg_assert(m_apClients[ClientId] == nullptr, "ssh_server connect failed ClientId %d reused", ClientId);

	log_info("ssh", "client with id %d connected", ClientId);

	CSshClient *pClient = new CSshClient(ClientId, Session);
	pClient->m_JoinTime = time_get();

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
				continue;
			}
		}

		if(!pClient->m_Authenticated)
		{
			if(!TryAuthenticateClient(pClient))
			{
				fprintf(stderr, "Authentication failed\n");
				OnClientDisconnect(pClient->m_ClientId);
				return;
			}
			continue;
		}

		if(pClient->m_Channel == nullptr)
		{
			if(TryOpenSessionChannel(pClient))
			{
				log_error("ssh", "failed to open channel");
				OnClientDisconnect(pClient->m_ClientId);
				return;
			}
		}
		if(!pClient->m_ShellReady)
		{
			if(!TryAcceptShell(pClient))
			{
				log_error("ssh", "shell request failed");
				OnClientDisconnect(pClient->m_ClientId);
				return;
			}
			continue;
		}

		if(!ssh_channel_is_open(pClient->m_Channel) || ssh_channel_is_eof(pClient->m_Channel))
		{
			ssh_channel_send_eof(pClient->m_Channel);
			ssh_channel_close(pClient->m_Channel);
			OnClientDisconnect(pClient->m_ClientId);
		}

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
