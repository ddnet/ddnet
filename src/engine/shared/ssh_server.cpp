#if defined(CONF_SSH)

#include "ssh_server.h"

#include <base/dbg.h>
#include <base/io.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

// TODO: the default file location in the current dir is not ideal
//       this should be in the storage save location
//       or maybe even use the system wide location that also the regular host
//       ssh server uses so we do not need to generate the key
#define HOSTKEY_FILE "ssh_host_rsa_key"

// the KEY_ prefix is already used by sdl
// also it does not seem to perfectly fit
// but idk what the better choice would be

#define KEY_ENTER 13
#define KEY_CTRL_U 21
#define KEY_CTRL_C 3
#define KEY_CTRL_D 4
#define KEY_DEL 127
#define KEY_BACKSPACE '\b'

// TODO: there is sockaddr_to_netaddr() in base/net.cpp but its static too there
//       moving it in to the header creates a diff to ddnet that is hard to maintain
//       also windows build is annoying because the signature requires includes
static void SockaddrToNetaddr(const sockaddr *pSrc, socklen_t SrcLen, NETADDR *pDst)
{
	*pDst = NETADDR_ZEROED;
	if(pSrc->sa_family == AF_INET && SrcLen >= (socklen_t)sizeof(sockaddr_in))
	{
		const sockaddr_in *pSrcIn = (const sockaddr_in *)pSrc;
		pDst->type = NETTYPE_IPV4;
		pDst->port = htons(pSrcIn->sin_port);
		static_assert(sizeof(pDst->ip) >= sizeof(pSrcIn->sin_addr.s_addr));
		mem_copy(pDst->ip, &pSrcIn->sin_addr.s_addr, sizeof(pSrcIn->sin_addr.s_addr));
	}
	else if(pSrc->sa_family == AF_INET6 && SrcLen >= (socklen_t)sizeof(sockaddr_in6))
	{
		const sockaddr_in6 *pSrcIn6 = (const sockaddr_in6 *)pSrc;
		pDst->type = NETTYPE_IPV6;
		pDst->port = htons(pSrcIn6->sin6_port);
		static_assert(sizeof(pDst->ip) >= sizeof(pSrcIn6->sin6_addr.s6_addr));
		mem_copy(pDst->ip, &pSrcIn6->sin6_addr.s6_addr, sizeof(pSrcIn6->sin6_addr.s6_addr));
	}
	else
	{
		log_warn("net", "Cannot convert sockaddr of family %d", pSrc->sa_family);
	}
}

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

void CSshServer::ListConnections()
{
	for(CSshClient *pClient : m_apClients)
	{
		if(!pClient)
			continue;

		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&pClient->m_Addr, aAddr, sizeof(aAddr), true);
		int64_t Seconds = (time_get() - pClient->m_JoinTime) / time_freq();
		log_info(
			"ssh",
			"id=%d addr=<{%s}> authed=%d online since %" PRId64 " seconds",
			pClient->m_ClientId,
			aAddr,
			pClient->m_Authenticated,
			Seconds);
	}
}

void CSshServer::HandleInput(CSshClient *pClient)
{
	ssh_channel Channel = pClient->m_Channel;
	char aBuf[256] = {0};

	int n = ssh_channel_read_nonblocking(Channel, aBuf, sizeof(aBuf), 0);
	if(n == SSH_EOF)
	{
		OnClientDisconnect(pClient->m_ClientId, "eof");
		return;
	}
	if(n == SSH_ERROR)
	{
		OnClientDisconnect(pClient->m_ClientId, "channel read error");
		return;
	}
	if(n == SSH_AGAIN || n == 0)
	{
		return;
	}

	int k = str_length(pClient->m_aInput);
	if(k + n > (int)sizeof(pClient->m_aInput) - 10)
	{
		// TODO: a \a bell would be nice in that case but I think it requires a PTY session

		// do not allow multiple characters at once to keep things simple
		if(n > 1)
			return;

		// allow operations that clear the input
		char Chr = aBuf[0];
		bool Whitelisted =
			Chr == KEY_ENTER ||
			Chr == KEY_CTRL_U ||
			Chr == KEY_CTRL_C ||
			Chr == KEY_CTRL_D ||
			Chr == KEY_DEL ||
			Chr == KEY_BACKSPACE;
		if(!Whitelisted)
			return;
	}

	// TODO: improve this shell!
	// TODO: arrow key up for history
	// TODO: ctrl+r history support
	// TODO: movement with arrow keys
	// TODO: word jumping and word deletion
	// TODO: autocomplete
	// TODO: can we use the readline library here somehow?

	for(int i = 0; i < n; i++)
	{
		char Byte = aBuf[i];
		if(Byte == KEY_ENTER)
		{
			const char *pCmd = pClient->m_aInput;
			if(pCmd[0])
			{
				log_info("ssh", "cid=%d cmd='%s'", pClient->m_ClientId, pCmd);

				if(!str_comp(pCmd, "logout") || !str_comp(pCmd, "exit"))
				{
					OnClientDisconnect(pClient->m_ClientId, "logout");
					return;
				}
				else if(!str_comp(pCmd, "w") || !str_comp(pCmd, "who"))
				{
					CSshLogger Logger(this, pClient->m_ClientId, log_get_scope_logger());
					CLogScope Scope(&Logger);
					ListConnections();
				}
				else
				{
					CSshLogger Logger(this, pClient->m_ClientId, log_get_scope_logger());
					CLogScope Scope(&Logger);
					Console()->ExecuteLine(pCmd, IConsole::CLIENT_ID_UNSPECIFIED, true);
				}
			}

			pClient->m_aInput[0] = '\0';
			ssh_channel_write(Channel, "\r\n> ", 4);
			return;
		}
		else if(Byte == KEY_CTRL_U)
		{
			// ideally this would not be the same as ctrl+c
			// and just clear the current prompt instead of
			// opening a new one
			pClient->m_aInput[0] = '\0';
			ssh_channel_write(Channel, "\r\n> ", 4);
			continue;
		}
		else if(Byte == KEY_CTRL_C)
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
		else if(Byte == KEY_CTRL_D)
		{
			// silently ignore ctrl+d if there is still input
			if(pClient->m_aInput[0])
				continue;

			OnClientDisconnect(pClient->m_ClientId, "logout");
			return;
		}
		else if(Byte == KEY_BACKSPACE || Byte == KEY_DEL)
		{
			int LastChr = str_length(pClient->m_aInput);
			LastChr = std::max(0, LastChr - 1);
			pClient->m_aInput[LastChr] = '\0';
			ssh_channel_write(pClient->m_Channel, "\b \b", 3);
			continue;
		}
		else if(Byte == 27) // escape sequence
		{
			if((n-i) < 2)
			{
				// this is odd, do we just ignore this one?
				continue;
			}
			// arrow keys
			if((n-i) >= 3 && aBuf[i+1] == 91)
			{
				if(aBuf[i+2] == 65) // arrow key up
				{
					// skip the sequence
					i += 2;
				}
				else if(aBuf[i+2] == 66) // arrow key down
				{
					// skip the sequence
					i += 2;
				}
			}

			// ignore unknown escape sequence for now
			continue;
		}

		pClient->m_aInput[k] = Byte;
		pClient->m_aInput[k + 1] = '\0';
		k++;
		ssh_channel_write(Channel, aBuf + i, 1);
	}

	// log_info("ssh", "got msg '%s' id=%d", aBuf, aBuf[0]);
	// log_info("ssh", " id=%d", aBuf[0]);
	// log_info("ssh", " id[0]=%d", aBuf[0]);
	// log_info("ssh", " id[1]=%d", aBuf[1]);
	// log_info("ssh", " id[2]=%d", aBuf[2]);
	// log_info("ssh", " n=%d", n);
	// log_info("ssh", " n=%d", n);
	// log_info("ssh", " n=%d", n);
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

void CSshServer::Init(CConfig *pConfig, IConsole *pConsole, IStorage *pStorage)
{
	m_pConfig = pConfig;
	m_pConsole = pConsole;
	m_pStorage = pStorage;

	if(!g_Config.m_SvSsh)
		return;

	log_info("ssh", "libssh %s", ssh_version(0));

	m_Bind = ssh_bind_new();
	if(m_Bind == nullptr)
	{
		str_copy(m_aError, "failed to bind");
		log_error("ssh", "failed to create ssh_bind");
		return;
	}

	GenerateHostKeyIfMissing();

	char aPort[32];
	str_format(aPort, sizeof(aPort), "%d", g_Config.m_SvSshPort);

	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0");
	ssh_bind_options_set(m_Bind, SSH_BIND_OPTIONS_BINDPORT_STR, aPort);
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

	log_info("ssh", "listening on 0.0.0.0:%s", aPort);
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

	if(g_Config.m_SvRconPassword[0] == '\0')
		return SSH_AUTH_DENIED;

	bool AdminUsername = str_comp(pUsername, "root") == 0 || str_comp(pUsername, "admin") == 0 || str_comp(pUsername, "default_admin") == 0;
	if(AdminUsername && str_comp(pPassword, g_Config.m_SvRconPassword) == 0)
	{
		pCtx->m_pClient->m_Authenticated = true;
		return SSH_AUTH_SUCCESS;
	}

	return SSH_AUTH_DENIED;
}

int CSshServer::AuthPubkeyCallback(ssh_session Session, const char *pUsername, struct ssh_key_struct *pClientPubKey, char SignatureState, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);
	IStorage *pStorage = pCtx->m_pServer->Storage();

	// TODO: the pUsername is not checked so a valid pub key can log into any name.
	//       is that convenient for the admin or insecure?
	//       right now there is only admin rank anyways

	const char *pAuthorizedKeysFile = "authorized_keys";

	CLineReader LineReader;
	if(!LineReader.OpenFile(pStorage->OpenFile(pAuthorizedKeysFile, IOFLAG_READ, IStorage::TYPE_SAVE)))
	{
		// no authorized_keys file
		return SSH_AUTH_DENIED;
	}

	bool Match = false;

	while(const char *pLine = LineReader.Get())
	{
		const char *pStr = str_skip_whitespaces_const(pLine);
		if(pStr[0] == '#')
			continue;

		char aKeyType[512] = {0};
		bool InvalidKey = false;
		size_t i;
		for(i = 0; i < sizeof(aKeyType); i++)
		{
			if(pStr[i] == ' ')
				break;
			if(pStr[i] == '\0')
			{
				InvalidKey = true;
				break;
			}

			aKeyType[i] = pStr[i];
		}
		if(InvalidKey)
			continue;
		pStr = str_skip_whitespaces_const(pStr + i);

		enum ssh_keytypes_e KeyType = ssh_key_type_from_name(aKeyType);
		if(KeyType == SSH_KEYTYPE_UNKNOWN)
		{
			continue;
		}

		// TODO: chop of the key name after the key otherwise the pubkey wont parse
		//       it has to be JUST the base64 key value

		ssh_key FileKey = nullptr;
		// Import public key from base64 string
		int Rc = ssh_pki_import_pubkey_base64(pStr, KeyType, &FileKey);
		if(Rc == SSH_OK && FileKey != nullptr)
		{
			// Compare the client's key with the key from the file
			if(ssh_key_cmp(pClientPubKey, FileKey, SSH_KEY_CMP_PUBLIC) == 0)
			{
				Match = true;
				ssh_key_free(FileKey);
				break;
			}
			ssh_key_free(FileKey);
		}
		else
		{
			log_error("ssh", "failed to read public key!");
			log_error("ssh", " return_code=%d", Rc);
			log_error("ssh", " key_type=%d", KeyType);
			log_error("ssh", " key='%s'", pStr);
		}
	}

	if(Match)
	{
		if(SignatureState == SSH_PUBLICKEY_STATE_NONE)
		{
			// non verified probe
			return SSH_AUTH_SUCCESS;
		}
		else if(SignatureState == SSH_PUBLICKEY_STATE_VALID)
		{
			// verified ownership -> authenticate
			pCtx->m_pClient->m_Authenticated = true;
			return SSH_AUTH_SUCCESS;
		}
	}

	return SSH_AUTH_DENIED;
}

ssh_channel CSshServer::ChannelOpenRequestSessionCallback(ssh_session Session, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		log_info("ssh", "Channel open DENIED: User not authenticated.");
		pCtx->m_pServer->OnClientDisconnect(pCtx->m_pClient->m_ClientId, "unauthed channel open");
		return nullptr;
	}

	ssh_channel Channel = ssh_channel_new(Session);
	if(Channel == nullptr)
	{
		pCtx->m_pServer->OnClientDisconnect(pCtx->m_pClient->m_ClientId, "failed to create channel");
		return nullptr;
	}

	pCtx->m_pClient->m_Channel = Channel;

	pCtx->m_pClient->m_ChannelCallback = {
		.userdata = pCtx,
		.channel_pty_request_function = ChannelPtyRequestCallback,
		.channel_shell_request_function = ChannelShellRequestCallback,
		.channel_pty_window_change_function = ChannelPtyWindowChangeCallback,
	};
	ssh_callbacks_init(&pCtx->m_pClient->m_ChannelCallback);
	ssh_set_channel_callbacks(Channel, &pCtx->m_pClient->m_ChannelCallback);

	return Channel;
}

int CSshServer::ChannelPtyRequestCallback(ssh_session Session, ssh_channel Channel, const char *pTerm, int Width, int Height, int PxWidth, int PwHeight, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx = static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		pCtx->m_pServer->OnClientDisconnect(pCtx->m_pClient->m_ClientId, "unauthed pty request");
		return SSH_ERROR;
	}

	// we don't support pty yet, only shell for now
	// but we need to return OK here otherwise the client shows an error
	return SSH_OK;
}

int CSshServer::ChannelPtyWindowChangeCallback(ssh_session Session, ssh_channel Channel, int Width, int Height, int PxWidth, int PwHeight, void *pUserData)
{
	return SSH_OK;
}

int CSshServer::ChannelShellRequestCallback(ssh_session Session, ssh_channel Channel, void *pUserData)
{
	CSshClient::CCallbackCtx *pCtx =
		static_cast<CSshClient::CCallbackCtx *>(pUserData);

	if(!pCtx->m_pClient->m_Authenticated)
	{
		// TODO: are these disconnects with error return really the way to go?
		//       is that safe or do we get into bad state if we drop a client from within the callback?
		pCtx->m_pServer->OnClientDisconnect(pCtx->m_pClient->m_ClientId, "unauthed shell request");
		return SSH_ERROR;
	}

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

	socket_t Socket = ssh_get_fd(Session);
	struct sockaddr_storage SockAddr;
	socklen_t SockAddrLen = sizeof(SockAddr);
	if(getpeername(Socket, (struct sockaddr *)&SockAddr, &SockAddrLen))
	{
		dbg_assert_failed("failed to get ssh connection address");
	}

	CSshClient *pClient = new CSshClient(ClientId, Session);
	pClient->m_JoinTime = time_get();

	SockaddrToNetaddr((sockaddr *)&SockAddr, SockAddrLen, &pClient->m_Addr);
	if(net_addr_comp(&pClient->m_Addr, &NETADDR_ZEROED) == 0)
	{
		dbg_assert_failed("failed to convert ssh client address");
	}

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&pClient->m_Addr, aAddr, sizeof(aAddr), true);

	// TODO: this prints for every connection attempt so attackers without the password can spam the log
	//       and occupy client ids
	//       so ideally there would be a different connection pool just for the before auth state
	log_info("ssh", "new connection cid=%d addr=<{%s}>", ClientId, aAddr);

	pClient->m_CallbackCtx = {
		.m_pClient = pClient,
		.m_pServer = this};

	pClient->m_ServerCallback = {
		.userdata = &pClient->m_CallbackCtx,
		.auth_password_function = AuthPasswordCallback,
		.auth_pubkey_function = AuthPubkeyCallback,
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

	log_info("ssh", "disconnect cid=%d reason='%s'", ClientId, pReason);

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

	int Rc = ssh_bind_accept(m_Bind, NewSession);
	if(Rc == SSH_ERROR)
	{
		ssh_free(NewSession);
		return;
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
	if(m_Bind == nullptr)
		return;
	if(g_Config.m_SvSsh == 0)
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
	{
		ssh_bind_free(m_Bind);
		m_Bind = nullptr;
	}
}

bool CSshServer::GotActiveConnections()
{
	return std::ranges::any_of(m_apClients, [](auto *pClient) { return pClient != nullptr; });
}

#endif
