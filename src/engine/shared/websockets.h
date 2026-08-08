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

// The websocket transport carries network chunks instead of whole Teeworlds
// packets ("ddnet-20" subprotocol): each binary websocket message is exactly
// one network chunk, a flags byte (`NET_CHUNKFLAG_VITAL`) followed by the
// chunk payload. An empty message is a keepalive. Reliability and ordering
// come from TCP, connect/close are the websocket connection itself, so there
// are no packet headers, security tokens, acknowledgements, resends or
// control messages; the vital flag is carried only for the message handlers
// that check it.

// NOLINTBEGIN(readability-identifier-naming)
enum websocket_event_type
{
	WEBSOCKET_EVENT_OPEN = 1,
	WEBSOCKET_EVENT_MESSAGE,
	WEBSOCKET_EVENT_CLOSE,
};

struct websocket_event
{
	websocket_event_type type;
	NETADDR addr;
	size_t size; // WEBSOCKET_EVENT_MESSAGE only, 0 means keepalive
	const unsigned char *data; // valid only until the next websocket_* call on this socket
	char close_reason[128]; // WEBSOCKET_EVENT_CLOSE only
};

#if defined(CONF_WEBSOCKETS)
void websocket_init();
int websocket_create(const NETADDR *bindaddr);
void websocket_destroy(int socket);
int websocket_connect(int socket, const NETADDR *addr);
// Cancel an in-flight client connect that has not completed yet
void websocket_connect_abort(int socket);
int websocket_recv_event(int socket, websocket_event *event);
// Receive the next event from any of a network socket's websocket transports
int websocket_recv_next(NETSOCKET sock, websocket_event *event, int *handle);
int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr);
// Close the connection, carrying the reason to the peer in the close frame.
// Produces no WEBSOCKET_EVENT_CLOSE: the caller must tear down its own
// connection state, and a deferred event would be misattributed to a new
// connection to the same address (e.g. an immediate reconnect)
void websocket_close(int socket, const NETADDR *addr, const char *reason);
int websocket_fd_set(int socket, fd_set *set);
int websocket_fd_get(int socket, fd_set *set);
#else
// Stubs so that the engine code needs no preprocessor guards
inline void websocket_init() {}
inline int websocket_create(const NETADDR *) { return -1; }
inline void websocket_destroy(int) {}
inline int websocket_connect(int, const NETADDR *) { return -1; }
inline void websocket_connect_abort(int) {}
inline int websocket_recv_event(int, websocket_event *) { return 0; }
inline int websocket_recv_next(NETSOCKET, websocket_event *, int *) { return 0; }
inline int websocket_send(int, const unsigned char *, size_t, const NETADDR *) { return -1; }
inline void websocket_close(int, const NETADDR *, const char *) {}
inline int websocket_fd_set(int, fd_set *) { return 0; }
inline int websocket_fd_get(int, fd_set *) { return 0; }
#endif
// NOLINTEND(readability-identifier-naming)

#endif // ENGINE_SHARED_WEBSOCKETS_H
