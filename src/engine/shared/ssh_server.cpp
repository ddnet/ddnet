#include "ssh_server.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/storage.h>

#include <fcntl.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT "2222"
#define HOSTKEY_FILE "ssh_host_rsa_key"
#define USERNAME "demo"
#define PASSWORD "secret"

static void cleanup_session(ssh_session session)
{
	if(session)
	{
		ssh_disconnect(session);
		ssh_free(session);
	}
}

static bool try_authenticate_client(CSshClient *pClient)
{
	ssh_session session = pClient->m_Session;
	ssh_message message = ssh_message_get(session);

	log_info("ssh", "trying to authenticate cid %d...", pClient->m_ClientId);

	if(message == nullptr)
		return true;

	if(ssh_message_type(message) == SSH_REQUEST_AUTH &&
		ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD)
	{
		const char *user = ssh_message_auth_user(message);
		const char *pass = ssh_message_auth_password(message);

		if(user && pass &&
			strcmp(user, USERNAME) == 0 &&
			strcmp(pass, PASSWORD) == 0)
		{
			ssh_message_auth_reply_success(message, 0);
			pClient->m_Authenticated = true;
		}
		else
		{
			ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
			ssh_message_reply_default(message);
		}
	}
	else
	{
		ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
		ssh_message_reply_default(message);
	}

	ssh_message_free(message);
	return true;
}

static ssh_channel open_session_channel(ssh_session session)
{
	ssh_message message;
	ssh_channel channel = NULL;

	while((message = ssh_message_get(session)) != NULL)
	{
		if(ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
			ssh_message_subtype(message) == SSH_CHANNEL_SESSION)
		{
			channel = ssh_message_channel_request_open_reply_accept(message);
			ssh_message_free(message);
			return channel;
		}

		ssh_message_reply_default(message);
		ssh_message_free(message);
	}

	return NULL;
}

static bool try_accept_shell(CSshClient *pClient)
{
	ssh_session session = pClient->m_Session;
	ssh_message message = ssh_message_get(session);

	log_info("ssh", "trying to shell ready cid %d...", pClient->m_ClientId);

	if(message == nullptr)
		return true;

	if(ssh_message_type(message) == SSH_REQUEST_CHANNEL)
	{
		switch(ssh_message_subtype(message))
		{
		case SSH_CHANNEL_REQUEST_PTY:
			ssh_message_channel_request_reply_success(message);
			break;

		case SSH_CHANNEL_REQUEST_SHELL:
			ssh_message_channel_request_reply_success(message);
			pClient->m_ShellReady = true;
			break;

		default:
			ssh_message_reply_default(message);
			break;
		}
	}
	else
	{
		ssh_message_reply_default(message);
	}

	ssh_message_free(message);
	return true;
}

static void run_echo_shell(ssh_channel channel)
{
	char buf[256];
	const char *banner =
		"Welcome to the toy SSH server.\r\n"
		"This is not a real shell.\r\n"
		"Anything you type will be echoed back.\r\n"
		"Type 'exit' to quit.\r\n\r\n";

	ssh_channel_write(channel, banner, strlen(banner));

	puts("read..");

	int n = ssh_channel_read_nonblocking(channel, buf, sizeof(buf), 0);
	if(n == SSH_EOF || n == SSH_ERROR)
	{
		puts("got some error");
		dbg_assert_failed("TODO: handle this ssh error");
		return;
	}
	if(n == SSH_AGAIN || n == 0)
	{
		return;
	}

	buf[n] = '\0';

	ssh_channel_write(channel, "echo: ", 6);
	ssh_channel_write(channel, buf, n);
}

/*
int main(void) {
    ssh_bind sshbind;
    int rc;

    sshbind = ssh_bind_new();
    if (sshbind == NULL) {
	fprintf(stderr, "Failed to create ssh_bind\n");
	return 1;
    }

    ssh_bind_set_blocking(sshbind, 0);

    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0");
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, PORT);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, HOSTKEY_FILE);

    rc = ssh_bind_listen(sshbind);
    if (rc < 0) {
	fprintf(stderr, "Listen error: %s\n", ssh_get_error(sshbind));
	ssh_bind_free(sshbind);
	return 1;
    }

    printf("Listening on 0.0.0.0:%s\n", PORT);
    printf("Username: %s\n", USERNAME);
    printf("Password: %s\n", PASSWORD);

    for (;;) {
	ssh_session session = NULL;
	ssh_channel channel = NULL;

	session = ssh_new();
	if (session == NULL) {
	    fprintf(stderr, "Failed to create session\n");
	    continue;
	}

	rc = ssh_bind_accept(sshbind, session);
	if (rc < 0) {
	    fprintf(stderr, "Accept error: %s\n", ssh_get_error(sshbind));
	    cleanup_session(session);
	    continue;
	}

	rc = ssh_handle_key_exchange(session);
	if (rc != SSH_OK) {
	    fprintf(stderr, "Key exchange failed: %s\n", ssh_get_error(session));
	    cleanup_session(session);
	    continue;
	}

	if (authenticate_client(session) != 0) {
	    fprintf(stderr, "Authentication failed\n");
	    cleanup_session(session);
	    continue;
	}

	channel = open_session_channel(session);
	if (channel == NULL) {
	    fprintf(stderr, "Failed to open channel\n");
	    cleanup_session(session);
	    continue;
	}

	puts("accept shell");

	if (accept_shell(session) != 0) {
	    fprintf(stderr, "Shell request failed\n");
	    ssh_channel_free(channel);
	    cleanup_session(session);
	    continue;
	}

	run_echo_shell(channel);

	ssh_channel_free(channel);
	cleanup_session(session);
    }

    ssh_bind_free(sshbind);
    return 0;
}
*/

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

	m_apClients[ClientId] = pClient;
}

void CSshServer::OnClientDisconnect(int ClientId)
{
	if(!m_apClients[ClientId])
		return;

	log_info("ssh", "client with id %d disconnected", ClientId);

	if(m_apClients[ClientId]->m_Channel)
		ssh_channel_free(m_apClients[ClientId]->m_Channel);
	cleanup_session(m_apClients[ClientId]->m_Session);

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

		if(!pClient->m_Authenticated)
		{
			if(!try_authenticate_client(pClient))
			{
				fprintf(stderr, "Authentication failed\n");
				OnClientDisconnect(pClient->m_ClientId);
				return;
			}
			continue;
		}

		if(pClient->m_Channel == nullptr)
		{
			pClient->m_Channel = open_session_channel(pClient->m_Session);
			if(pClient->m_Channel == nullptr)
			{
				fprintf(stderr, "Failed to open channel\n");
				OnClientDisconnect(pClient->m_ClientId);
				return;
			}
		}
		if(!pClient->m_ShellReady)
		{
			if(!try_accept_shell(pClient))
			{
				fprintf(stderr, "Shell request failed\n");
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

		run_echo_shell(pClient->m_Channel);
	}
}

void CSshServer::Shutdown()
{
	if(m_Bind != nullptr && m_aError[0] == '\0')
		ssh_bind_free(m_Bind);
}
