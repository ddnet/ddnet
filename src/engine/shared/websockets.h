#ifndef ENGINE_SHARED_WEBSOCKETS_H
#define ENGINE_SHARED_WEBSOCKETS_H

#if defined(CONF_FAMILY_UNIX)
#elif defined(CONF_FAMILY_WINDOWS)
#include <winsock2.h>
#endif

#include <cstddef>

int websocket_create(const char *addr, int port);
int websocket_destroy(int socket);
int websocket_recv(int socket, unsigned char *data, size_t maxsize, struct sockaddr_in *sockaddrbuf, size_t fromLen);
int websocket_send(int socket, const unsigned char *data, size_t size,
	const char *addr_str, int port);
int websocket_fd_set(int socket, fd_set *set);

#endif // ENGINE_SHARED_WEBSOCKETS_H
