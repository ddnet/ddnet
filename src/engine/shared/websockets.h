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
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)
void websocket_init();
int websocket_create(const NETADDR *bindaddr);
void websocket_destroy(int socket);
int websocket_recv(int socket, unsigned char *data, size_t maxsize, NETADDR *addr);
int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr);
#if defined(CONF_PLATFORM_EMSCRIPTEN)
/**
 * Waits until data is available on one of the sockets or until the timeout expires.
 * Control is returned to the JavaScript event loop while waiting, which is
 * necessary for receiving data, because `select` cannot block with Emscripten.
 *
 * @param sockets Sockets to wait for, negative values are ignored.
 * @param num_sockets Number of sockets.
 * @param microseconds Maximum time to wait.
 *
 * @return `1` if data is available, `0` otherwise.
 */
int websocket_wait(const int *sockets, int num_sockets, int64_t microseconds);
#else
int websocket_fd_set(int socket, fd_set *set);
int websocket_fd_get(int socket, fd_set *set);
#endif
// NOLINTEND(readability-identifier-naming)

#endif // ENGINE_SHARED_WEBSOCKETS_H
