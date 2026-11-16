#include "websockets.h"

#if defined(CONF_WEBSOCKETS) && defined(CONF_PLATFORM_EMSCRIPTEN)

#include <base/net.h>

#include <algorithm>
#include <limits>

// The WebSocket connections are managed with JavaScript, see websockets_emscripten.js.
// The connections are accepted by the scripts in other/emscripten/server, which run
// the server with Node.js or in a Cloudflare Durable Object.
extern "C" {
int WebsocketCreateJsImpl(int Port, bool Ipv6);
void WebsocketDestroyJsImpl(int Socket);
int WebsocketRecvJsImpl(int Socket, unsigned char *pData, int MaxSize, unsigned char *pIp, unsigned short *pPort, bool *pIpv6);
int WebsocketSendJsImpl(int Socket, const unsigned char *pData, int Size, const unsigned char *pIp, unsigned short Port);
int WebsocketWaitJsImpl(const int *pSockets, int NumSockets, int TimeoutMs);
}

// NOLINTBEGIN(readability-identifier-naming)
void websocket_init()
{
}

int websocket_create(const NETADDR *bindaddr)
{
	return WebsocketCreateJsImpl(bindaddr->port, bindaddr->type == NETTYPE_WEBSOCKET_IPV6);
}

void websocket_destroy(int socket)
{
	WebsocketDestroyJsImpl(socket);
}

int websocket_recv(int socket, unsigned char *data, size_t maxsize, NETADDR *addr)
{
	*addr = NETADDR_ZEROED;
	bool ipv6 = false;
	const int size = WebsocketRecvJsImpl(socket, data, std::min<size_t>(maxsize, std::numeric_limits<int>::max()), addr->ip, &addr->port, &ipv6);
	if(size > 0)
	{
		addr->type = ipv6 ? NETTYPE_WEBSOCKET_IPV6 : NETTYPE_WEBSOCKET_IPV4;
	}
	return size;
}

int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr)
{
	return WebsocketSendJsImpl(socket, data, size, addr->ip, addr->port);
}

int websocket_wait(const int *sockets, int num_sockets, int64_t microseconds)
{
	// Round up, so the wait does not return before the timeout expires
	const int64_t milliseconds = std::min<int64_t>((microseconds + 999) / 1000, std::numeric_limits<int>::max());
	return WebsocketWaitJsImpl(sockets, num_sockets, milliseconds);
}
// NOLINTEND(readability-identifier-naming)

#endif
