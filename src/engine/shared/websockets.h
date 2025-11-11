#ifndef ENGINE_SHARED_WEBSOCKETS_H
#define ENGINE_SHARED_WEBSOCKETS_H

#include <base/detect.h>
#include <base/types.h>

#if defined(CONF_FAMILY_UNIX)
#include <sys/select.h>
#elif defined(CONF_FAMILY_WINDOWS)
#include <winsock2.h>
#endif

#include <cstddef>

void websocket_init();
int websocket_create(const NETADDR *bindaddr);
void websocket_destroy(int socket);
int websocket_recv(int socket, unsigned char *data, size_t maxsize, NETADDR *addr);
int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr);
int websocket_fd_set(int socket, fd_set *set);
int websocket_fd_get(int socket, fd_set *set);

#endif // ENGINE_SHARED_WEBSOCKETS_H
