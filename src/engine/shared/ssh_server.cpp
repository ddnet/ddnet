#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT "2222"
#define HOSTKEY_FILE "ssh_host_rsa_key"
#define USERNAME "demo"
#define PASSWORD "secret"

static void cleanup_session(ssh_session session) {
    if (session) {
        ssh_disconnect(session);
        ssh_free(session);
    }
}

static int authenticate_client(ssh_session session) {
    ssh_message message;
    int authenticated = 0;

    while (!authenticated && (message = ssh_message_get(session)) != NULL) {
        if (ssh_message_type(message) == SSH_REQUEST_AUTH &&
            ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD) {

            const char *user = ssh_message_auth_user(message);
            const char *pass = ssh_message_auth_password(message);

            if (user && pass &&
                strcmp(user, USERNAME) == 0 &&
                strcmp(pass, PASSWORD) == 0) {
                ssh_message_auth_reply_success(message, 0);
                authenticated = 1;
            } else {
                ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
                ssh_message_reply_default(message);
            }
        } else {
            ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
            ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    }

    return authenticated ? 0 : -1;
}

static ssh_channel open_session_channel(ssh_session session) {
    ssh_message message;
    ssh_channel channel = NULL;

    while ((message = ssh_message_get(session)) != NULL) {
        if (ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
            ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
            channel = ssh_message_channel_request_open_reply_accept(message);
            ssh_message_free(message);
            return channel;
        }

        ssh_message_reply_default(message);
        ssh_message_free(message);
    }

    return NULL;
}

static int accept_shell(ssh_session session) {
    ssh_message message;
    int shell_ready = 0;

    while (!shell_ready && (message = ssh_message_get(session)) != NULL) {
        if (ssh_message_type(message) == SSH_REQUEST_CHANNEL) {
            switch (ssh_message_subtype(message)) {
                case SSH_CHANNEL_REQUEST_PTY:
                    ssh_message_channel_request_reply_success(message);
                    break;

                case SSH_CHANNEL_REQUEST_SHELL:
                    ssh_message_channel_request_reply_success(message);
                    shell_ready = 1;
                    break;

                default:
                    ssh_message_reply_default(message);
                    break;
            }
        } else {
            ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    }

    return shell_ready ? 0 : -1;
}

static void run_echo_shell(ssh_channel channel) {
    char buf[256];
    const char *banner =
        "Welcome to the toy SSH server.\r\n"
        "This is not a real shell.\r\n"
        "Anything you type will be echoed back.\r\n"
        "Type 'exit' to quit.\r\n\r\n";

    ssh_channel_write(channel, banner, strlen(banner));

    while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel)) {
        int n = ssh_channel_read(channel, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            break;
        }

        buf[n] = '\0';

        if (strcmp(buf, "exit\n") == 0 || strcmp(buf, "exit\r\n") == 0) {
            break;
        }

        ssh_channel_write(channel, "echo: ", 6);
        ssh_channel_write(channel, buf, n);
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
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
