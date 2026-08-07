#if defined(CONF_WEBSOCKETS)

#include "websockets.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/protocol.h>
#include <engine/shared/ringbuffer.h>

#if defined(CONF_FAMILY_UNIX)
#include <arpa/inet.h>
#elif defined(CONF_FAMILY_WINDOWS)
#include <ws2tcpip.h>
#endif
#include <libwebsockets.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
static const char PROTOCOL_NAME[] = "ddnet-20";

struct websocket_chunk
{
	int type; // websocket_event_type
	bool abrupt; // WEBSOCKET_EVENT_CLOSE only
	size_t size;
	NETADDR addr;
	unsigned char data[0]; // message payload or close reason
};

// Shared by all sessions of a context (client opens two connections for
// whatever reason). No FLAG_RECYCLE: recycling would evict other sessions'
// still-undelivered events, and the protocol has no retransmission to recover
// them, so a full buffer closes the offending connection instead (see the
// push_event callers).
typedef CStaticRingBuffer<websocket_chunk, (MAX_CLIENTS * 2) * NET_CONN_BUFFERSIZE>
	TRecvBuffer;
// No FLAG_RECYCLE: recycling could evict a partially transmitted chunk, corrupting the frame stream
typedef CStaticRingBuffer<websocket_chunk, NET_CONN_BUFFERSIZE>
	TSendBuffer;

struct per_session_data
{
	lws *wsi;
	NETADDR addr;
	bool registered;
	bool close_requested;
	bool notify_close; // deliver a CLOSE event when the session closes
	bool peer_closed; // the peer announced the close with a close frame
	char close_reason[128];
	// One message is delivered by lws in multiple pieces when its frames span
	// multiple socket reads, so it is reassembled here before delivery
	size_t recv_size;
	unsigned char recv_buffer[1 + NET_MAX_PAYLOAD];
	TSendBuffer send_buffer;
};

struct context_data
{
	char bindaddr_str[NETADDR_MAXSTRSIZE];
	lws_context_creation_info creation_info;
	lws_context *context;
	std::map<NETADDR, per_session_data *> port_map;
	TRecvBuffer recv_buffer;
	bool event_pending_pop;
	lws *client_wsi; // in-flight client connect, until it establishes or fails
	NETADDR client_target; // address the in-flight client connect was initiated to
};

// Client has main, dummy and contact connections with IPv4 and IPv6
static context_data contexts[3 * 2];
static std::map<lws_context *, context_data *> contexts_map;

static lws_context *websocket_context(int socket)
{
	if(socket < 0)
		return nullptr;
	dbg_assert(socket < (int)std::size(contexts), "socket index invalid: %d", socket);
	lws_context *context = contexts[socket].context;
	dbg_assert(context != nullptr, "socket context not initialized: %d", socket);
	return context;
}

static bool push_event(context_data *ctx_data, int type, const NETADDR *addr, const void *data, size_t size, bool abrupt = false)
{
	websocket_chunk *chunk = ctx_data->recv_buffer.Allocate(size + sizeof(websocket_chunk));
	if(chunk == nullptr)
		return false;
	chunk->type = type;
	chunk->abrupt = abrupt;
	chunk->size = size;
	chunk->addr = *addr;
	if(size > 0)
	{
		mem_copy(&chunk->data[0], data, size);
	}
	return true;
}

static void sockaddr_to_netaddr_websocket(const sockaddr *src, socklen_t src_len, NETADDR *dst)
{
	*dst = NETADDR_ZEROED;
	if(src->sa_family == AF_INET && src_len >= (socklen_t)sizeof(sockaddr_in))
	{
		const sockaddr_in *src_in = (const sockaddr_in *)src;
		dst->type = NETTYPE_WEBSOCKET_IPV4;
		dst->port = htons(src_in->sin_port);
		static_assert(sizeof(dst->ip) >= sizeof(src_in->sin_addr.s_addr));
		mem_copy(dst->ip, &src_in->sin_addr.s_addr, sizeof(src_in->sin_addr.s_addr));
	}
	else if(src->sa_family == AF_INET6 && src_len >= (socklen_t)sizeof(sockaddr_in6))
	{
		const sockaddr_in6 *src_in6 = (const sockaddr_in6 *)src;
		dst->type = NETTYPE_WEBSOCKET_IPV6;
		dst->port = htons(src_in6->sin6_port);
		static_assert(sizeof(dst->ip) >= sizeof(src_in6->sin6_addr.s6_addr));
		mem_copy(dst->ip, &src_in6->sin6_addr.s6_addr, sizeof(src_in6->sin6_addr.s6_addr));
	}
	else
	{
		log_warn("websockets", "Cannot convert sockaddr of family %d", src->sa_family);
	}
}

static bool protocol_offered(lws *wsi)
{
	char offers[256];
	if(lws_hdr_copy(wsi, offers, sizeof(offers), WSI_TOKEN_PROTOCOL) <= 0)
	{
		return false;
	}
	const char *next = offers;
	char offer[64];
	while((next = str_next_token(next, ",", offer, sizeof(offer))))
	{
		if(str_comp(str_skip_whitespaces(offer), PROTOCOL_NAME) == 0)
		{
			return true;
		}
	}
	return false;
}

// Returns false when the OPEN event could not be queued (receive buffer full);
// the caller must then close the connection so no half-registered session leaks
static bool session_open(context_data *ctx_data, per_session_data *pss, lws *wsi, const NETADDR *addr)
{
	// Deliver OPEN before registering, so a failure leaves nothing behind
	if(!push_event(ctx_data, WEBSOCKET_EVENT_OPEN, addr, nullptr, 0))
	{
		return false;
	}
	pss->wsi = wsi;
	pss->addr = *addr;
	pss->registered = true;
	pss->close_requested = false;
	pss->notify_close = true;
	pss->peer_closed = false;
	pss->close_reason[0] = '\0';
	pss->recv_size = 0;
	pss->send_buffer.Init();
	ctx_data->port_map[*addr] = pss;

	char addr_str[NETADDR_MAXSTRSIZE];
	net_addr_str(addr, addr_str, sizeof(addr_str), true);
	log_trace("websockets", "Connection established with '%s'", addr_str);
	return true;
}

static void session_close(context_data *ctx_data, per_session_data *pss)
{
	if(!pss->registered)
		return;
	pss->registered = false;
	pss->wsi = nullptr;
	// Only remove our own entry: a new session may already have reused this
	// address after an out-of-order reconnect
	auto it = ctx_data->port_map.find(pss->addr);
	if(it != ctx_data->port_map.end() && it->second == pss)
	{
		ctx_data->port_map.erase(it);
	}

	char addr_str[NETADDR_MAXSTRSIZE];
	net_addr_str(&pss->addr, addr_str, sizeof(addr_str), true);
	// No close event for closes the engine itself requested via
	// websocket_close: the caller has already torn down its connection state,
	// and delivering the event later would misattribute it to a new
	// connection to the same address (e.g. an immediate reconnect)
	if(pss->notify_close && !push_event(ctx_data, WEBSOCKET_EVENT_CLOSE, &pss->addr, pss->close_reason, str_length(pss->close_reason) + 1, !pss->peer_closed))
	{
		// The engine will not learn the session closed and drops it on timeout
		log_error("websockets", "Failed to deliver close event for '%s', receive buffer full", addr_str);
	}
	log_trace("websockets", "Connection closed with '%s'", addr_str);
}

static void request_session_close(per_session_data *pss, const char *reason, bool notify_engine)
{
	if(pss->close_requested)
	{
		// A close is already pending, keep its reason and the writeable
		// callback that was armed for it. Only the notification is downgraded:
		// the engine must not get a close event for a close it requested
		// itself, even when the transport requested one first.
		pss->notify_close = pss->notify_close && notify_engine;
		return;
	}
	pss->close_requested = true;
	pss->notify_close = notify_engine;
	str_copy(pss->close_reason, reason != nullptr ? reason : "");
	lws_callback_on_writable(pss->wsi);
}

static int websocket_protocol_callback(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	per_session_data *pss = (per_session_data *)user;
	lws_context *context = lws_get_context(wsi);
	context_data *ctx_data = contexts_map[context];
	switch(reason)
	{
	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		// Require the client to explicitly request the ddnet-20 subprotocol
		return protocol_offered(wsi) ? 0 : -1;

	case LWS_CALLBACK_ESTABLISHED:
	{
		sockaddr_storage peersockaddr;
		socklen_t peersockaddr_size = sizeof(peersockaddr);
		getpeername(lws_get_socket_fd(wsi), (sockaddr *)&peersockaddr, &peersockaddr_size);
		NETADDR addr;
		sockaddr_to_netaddr_websocket((sockaddr *)&peersockaddr, peersockaddr_size, &addr);
		if(addr.type == NETTYPE_INVALID)
		{
			return -1;
		}
		// Close the connection if the OPEN event cannot be delivered
		return session_open(ctx_data, pss, wsi, &addr) ? 0 : -1;
	}

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		// Only accept the connect we are currently waiting for. A stale attempt
		// (aborted, or a second connect started meanwhile) that completes late
		// must not be registered under the current target's address.
		if(wsi != ctx_data->client_wsi)
		{
			return -1;
		}
		ctx_data->client_wsi = nullptr;
		return session_open(ctx_data, pss, wsi, &ctx_data->client_target) ? 0 : -1;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	{
		// Ignore errors from aborted or already-completed attempts, they no
		// longer correspond to client_target
		if(wsi != ctx_data->client_wsi)
		{
			return 0;
		}
		ctx_data->client_wsi = nullptr;
		const char *error = in != nullptr ? (const char *)in : "connection failed";
		push_event(ctx_data, WEBSOCKET_EVENT_CLOSE, &ctx_data->client_target, error, str_length(error) + 1);
		return 0;
	}

	case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
		// The connection is over as soon as the peer announces the close, the
		// transport finishes the close handshake underneath. The close frame
		// payload is a two byte status code followed by the reason.
		if(pss != nullptr)
		{
			pss->peer_closed = true;
			if(len > 2)
			{
				const size_t reason_len = std::min(len - 2, sizeof(pss->close_reason) - 1);
				mem_copy(pss->close_reason, (const char *)in + 2, reason_len);
				pss->close_reason[reason_len] = '\0';
				str_sanitize_cc(pss->close_reason);
			}
			session_close(ctx_data, pss);
		}
		return 0;

	case LWS_CALLBACK_CLIENT_CLOSED:
		[[fallthrough]];
	case LWS_CALLBACK_CLOSED:
		[[fallthrough]];
	case LWS_CALLBACK_WSI_DESTROY:
		// Never leave a dangling pointer to a destroyed in-flight connect
		if(wsi == ctx_data->client_wsi)
		{
			ctx_data->client_wsi = nullptr;
		}
		if(pss != nullptr)
		{
			session_close(ctx_data, pss);
		}
		return 0;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		[[fallthrough]];
	case LWS_CALLBACK_SERVER_WRITEABLE:
	{
		if(pss->close_requested)
		{
			lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)pss->close_reason, std::min<size_t>(str_length(pss->close_reason), 123));
			return -1;
		}

		websocket_chunk *chunk = pss->send_buffer.First();
		if(chunk == nullptr)
		{
			return 0;
		}

		// lws buffers any unsent remainder of a websocket frame internally,
		// so a short write means the connection is unusable
		int n = lws_write(wsi, &chunk->data[LWS_SEND_BUFFER_PRE_PADDING], chunk->size, LWS_WRITE_BINARY);
		if(n < (int)chunk->size)
		{
			return -1;
		}

		pss->send_buffer.PopFirst();
		lws_callback_on_writable(wsi);
		return 0;
	}

	case LWS_CALLBACK_CLIENT_RECEIVE:
		[[fallthrough]];
	case LWS_CALLBACK_RECEIVE:
	{
		// Reassemble the message: lws delivers it in multiple pieces when its
		// frames span multiple socket reads. Close the connection on messages
		// larger than the flags byte plus the maximum chunk payload, they can
		// never be valid chunks. An empty message is a keepalive.
		if(pss->recv_size + len > sizeof(pss->recv_buffer))
		{
			char addr_str[NETADDR_MAXSTRSIZE];
			net_addr_str(&pss->addr, addr_str, sizeof(addr_str), true);
			log_error("websockets", "Closing connection with '%s' due to oversized message of size %" PRIzu, addr_str, pss->recv_size + len);
			return -1;
		}
		if(len > 0)
		{
			mem_copy(&pss->recv_buffer[pss->recv_size], in, len);
			pss->recv_size += len;
		}
		if(!lws_is_final_fragment(wsi) || lws_remaining_packet_payload(wsi) > 0)
		{
			return 0;
		}
		const bool pushed = push_event(ctx_data, WEBSOCKET_EVENT_MESSAGE, &pss->addr, pss->recv_buffer, pss->recv_size);
		pss->recv_size = 0;
		if(!pushed)
		{
			// Receive buffer full: the reliable stream cannot be preserved
			return -1;
		}
		return 0;
	}

	default:
		return 0;
	}
}

static const lws_protocols protocols[] = {
	{PROTOCOL_NAME, websocket_protocol_callback, sizeof(per_session_data)},
	{nullptr, nullptr, 0}};

static LEVEL websocket_level_to_loglevel(int level)
{
	switch(level)
	{
	case LLL_ERR:
		return LEVEL_ERROR;
	case LLL_WARN:
		return LEVEL_WARN;
	case LLL_NOTICE:
	case LLL_INFO:
		return LEVEL_DEBUG;
	default:
		dbg_assert_failed("invalid log level: %d", level);
	}
}

static void websocket_log_callback(int level, const char *line)
{
	if((level == LLL_NOTICE || level == LLL_INFO) && !g_Config.m_DbgWebsockets)
	{
		return;
	}

	// Truncate duplicate timestamp from beginning and newline from end
	char line_truncated[4096]; // Longest log line length
	const char *line_time_end = str_find(line, "] ");
	dbg_assert(line_time_end != nullptr, "unexpected log format");
	str_copy(line_truncated, line_time_end + 2);
	const int length = str_length(line_truncated);
	if(line_truncated[length - 1] == '\n')
	{
		line_truncated[length - 1] = '\0';
	}
	if(line_truncated[length - 2] == '\r')
	{
		line_truncated[length - 2] = '\0';
	}
	log_log(websocket_level_to_loglevel(level), "websockets", "%s", line_truncated);
}

void websocket_init()
{
	lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO, websocket_log_callback);
}

int websocket_create(const NETADDR *bindaddr)
{
	// find free context
	int first_free = -1;
	for(int i = 0; i < (int)std::size(contexts); i++)
	{
		if(contexts[i].context == nullptr)
		{
			first_free = i;
			break;
		}
	}
	if(first_free == -1)
	{
		log_error("websockets", "Failed to create websocket: no free contexts available");
		return -1;
	}

	context_data *ctx_data = &contexts[first_free];
	mem_zero(&ctx_data->creation_info, sizeof(ctx_data->creation_info));
	ctx_data->creation_info.options = LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND;
	if(bindaddr->type == NETTYPE_WEBSOCKET_IPV6)
	{
		// Set IPv6-only mode and socket option for IPv6 Websockets.
		ctx_data->creation_info.options |= LWS_SERVER_OPTION_IPV6_V6ONLY_VALUE | LWS_SERVER_OPTION_IPV6_V6ONLY_MODIFY;
	}
	net_addr_str(bindaddr, ctx_data->bindaddr_str, sizeof(ctx_data->bindaddr_str), false);
	if(ctx_data->bindaddr_str[0] == '[' && ctx_data->bindaddr_str[str_length(ctx_data->bindaddr_str) - 1] == ']')
	{
		// Bindaddr must not be enclosed in brackets for IPv6 Websockets.
		ctx_data->bindaddr_str[str_length(ctx_data->bindaddr_str) - 1] = '\0';
		mem_move(&ctx_data->bindaddr_str[0], &ctx_data->bindaddr_str[1], str_length(ctx_data->bindaddr_str) + 1);
	}
	ctx_data->creation_info.iface = ctx_data->bindaddr_str;
	ctx_data->creation_info.port = bindaddr->port;
	ctx_data->creation_info.protocols = protocols;
	ctx_data->creation_info.gid = -1;
	ctx_data->creation_info.uid = -1;
	ctx_data->creation_info.user = ctx_data;

	ctx_data->context = lws_create_context(&ctx_data->creation_info);
	if(ctx_data->context == nullptr)
	{
		return -1;
	}
	contexts_map[ctx_data->context] = ctx_data;
	ctx_data->recv_buffer.Init();
	ctx_data->event_pending_pop = false;
	ctx_data->client_wsi = nullptr;
	ctx_data->client_target = NETADDR_ZEROED;
	return first_free;
}

void websocket_destroy(int socket)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return;
	lws_context_destroy(context);
	contexts_map.erase(context);
	contexts[socket].context = nullptr;
}

int websocket_connect(int socket, const NETADDR *addr)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return -1;
	context_data *ctx_data = contexts_map[context];

	char addr_str[NETADDR_MAXSTRSIZE];
	net_addr_str(addr, addr_str, sizeof(addr_str), false);
	if(addr_str[0] == '[' && addr_str[str_length(addr_str) - 1] == ']')
	{
		// Address must not be enclosed in brackets for IPv6 Websockets.
		addr_str[str_length(addr_str) - 1] = '\0';
		mem_move(&addr_str[0], &addr_str[1], str_length(addr_str) + 1);
	}

	ctx_data->client_target = *addr;

	lws_client_connect_info ccinfo = {};
	ccinfo.context = context;
	ccinfo.address = addr_str;
	ccinfo.port = addr->port;
	ccinfo.path = "/";
	ccinfo.host = addr_str;
	ccinfo.origin = addr_str;
	ccinfo.protocol = PROTOCOL_NAME;
	lws *wsi = lws_client_connect_via_info(&ccinfo);
	if(wsi == nullptr)
	{
		ctx_data->client_target = NETADDR_ZEROED;
		return -1;
	}
	ctx_data->client_wsi = wsi;
	lws_service(context, -1);
	return 0;
}

void websocket_connect_abort(int socket)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return;
	context_data *ctx_data = contexts_map[context];
	lws *wsi = ctx_data->client_wsi;
	// Clear first so the resulting error/destroy callbacks treat it as stale
	ctx_data->client_wsi = nullptr;
	ctx_data->client_target = NETADDR_ZEROED;
	if(wsi != nullptr)
	{
		// Without this the wsi lingers in lws for its full connect timeout, and
		// its late completion or error would be misattributed to the next
		// connect target. Closing synchronously is safe here: this is not
		// called from within a callback for this wsi.
		lws_set_timeout(wsi, PENDING_TIMEOUT_USER_OK, LWS_TO_KILL_SYNC);
	}
}

int websocket_recv_event(int socket, websocket_event *event)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return -1;
	context_data *ctx_data = contexts_map[context];

	// Free the previously delivered event before servicing, so a re-entrant
	// service (from a nested websocket_send/close) can never pop a live event
	if(ctx_data->event_pending_pop)
	{
		ctx_data->recv_buffer.PopFirst();
		ctx_data->event_pending_pop = false;
	}

	// Only service when nothing is buffered, so one service call drains every
	// event it produced instead of costing one service (a poll over all
	// sessions) per delivered event
	if(ctx_data->recv_buffer.First() == nullptr)
	{
		const int service_result = lws_service(context, -1);
		if(service_result < 0)
		{
			return service_result;
		}
	}

	websocket_chunk *chunk = ctx_data->recv_buffer.First();
	if(chunk == nullptr)
	{
		return 0;
	}
	ctx_data->event_pending_pop = true;

	event->type = (websocket_event_type)chunk->type;
	event->addr = chunk->addr;
	event->size = 0;
	event->data = nullptr;
	event->close_reason[0] = '\0';
	event->abrupt = false;
	if(chunk->type == WEBSOCKET_EVENT_MESSAGE)
	{
		event->size = chunk->size;
		event->data = &chunk->data[0];
	}
	else if(chunk->type == WEBSOCKET_EVENT_CLOSE)
	{
		event->abrupt = chunk->abrupt;
		if(chunk->size > 0)
		{
			str_copy(event->close_reason, (const char *)&chunk->data[0], std::min(sizeof(event->close_reason), chunk->size));
		}
	}
	return 1;
}

int websocket_recv_next(NETSOCKET sock, websocket_event *event, int *handle)
{
	for(const int nettype : {NETTYPE_WEBSOCKET_IPV4, NETTYPE_WEBSOCKET_IPV6})
	{
		*handle = net_socket_websocket(sock, nettype);
		if(*handle >= 0 && websocket_recv_event(*handle, event) > 0)
			return 1;
	}
	return 0;
}

int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr, bool vital)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return -1;
	context_data *ctx_data = contexts_map[context];
	auto it = ctx_data->port_map.find(*addr);
	if(it == ctx_data->port_map.end() || it->second == nullptr || it->second->wsi == nullptr)
	{
		return -1;
	}
	per_session_data *pss = it->second;

	const size_t chunk_size = size + sizeof(websocket_chunk) + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING;
	websocket_chunk *chunk = pss->send_buffer.Allocate(chunk_size);
	if(chunk == nullptr)
	{
		// The send buffer is full, so the transport is stalled. Drop the
		// message if it may be lost, exactly like the network drops a UDP
		// packet: the peer misses one snapshot and keeps playing.
		if(!vital)
		{
			return size;
		}
		// Unlike UDP there is no retransmission, so dropping a vital message
		// would silently and permanently break the reliable stream; close the
		// connection instead. The engine did not ask for this close, so it
		// gets the close event.
		request_session_close(pss, "Too weak connection", true);
		return -1;
	}
	mem_zero(chunk, chunk_size);
	chunk->size = size;
	chunk->addr = pss->addr;
	if(size > 0)
	{
		// data is null for the empty keepalive message
		mem_copy(&chunk->data[LWS_SEND_BUFFER_PRE_PADDING], data, size);
	}
	lws_callback_on_writable(pss->wsi);
	lws_service(context, -1);
	return size;
}

void websocket_close(int socket, const NETADDR *addr, const char *reason)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return;
	context_data *ctx_data = contexts_map[context];
	auto it = ctx_data->port_map.find(*addr);
	if(it == ctx_data->port_map.end() || it->second == nullptr || it->second->wsi == nullptr)
	{
		return;
	}
	per_session_data *pss = it->second;
	request_session_close(pss, reason, false);
	// Pump until the close frame is flushed; the session is removed from the
	// port map once the connection is fully closed
	for(int i = 0; i < 10 && ctx_data->port_map.contains(*addr); i++)
	{
		lws_service(context, -1);
	}
}

int websocket_fd_set(int socket, fd_set *set)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return 0;
	lws_service(context, -1);

	context_data *ctx_data = contexts_map[context];
	int max = 0;
	for(const auto &[_, pss] : ctx_data->port_map)
	{
		if(pss == nullptr)
		{
			continue;
		}
		int fd = lws_get_socket_fd(pss->wsi);
		max = std::max(fd, max);
		FD_SET(fd, set);
	}
	return max;
}

int websocket_fd_get(int socket, fd_set *set)
{
	lws_context *context = websocket_context(socket);
	if(context == nullptr)
		return 0;
	lws_service(context, -1);

	context_data *ctx_data = contexts_map[context];
	// Undelivered events must wake the engine even though the session they
	// belong to may already be gone from the port map: the close event of a
	// disconnected peer has no fd left that could report activity
	if(ctx_data->recv_buffer.First() != nullptr)
	{
		return 1;
	}
	for(const auto &[_, pss] : ctx_data->port_map)
	{
		if(pss == nullptr)
		{
			continue;
		}
		if(FD_ISSET(lws_get_socket_fd(pss->wsi), set))
		{
			return 1;
		}
	}
	return 0;
}
// NOLINTEND(readability-identifier-naming)

#endif
