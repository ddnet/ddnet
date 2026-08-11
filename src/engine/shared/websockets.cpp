#if defined(CONF_WEBSOCKETS)

#include "websockets.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/str.h>
#include <base/time.h>

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

#include <cstdlib>
#include <map>
#include <set>
#include <string>

// NOLINTBEGIN(readability-identifier-naming)
struct websocket_chunk
{
	size_t size;
	size_t read;
	NETADDR addr;
	unsigned char data[0];
};

// Client opens two connections for whatever reason
typedef CStaticRingBuffer<websocket_chunk, (MAX_CLIENTS * 2) * NET_CONN_BUFFERSIZE,
	CRingBufferBase::FLAG_RECYCLE>
	TRecvBuffer;
typedef CStaticRingBuffer<websocket_chunk, NET_CONN_BUFFERSIZE,
	CRingBufferBase::FLAG_RECYCLE>
	TSendBuffer;

struct per_session_data
{
	lws *wsi;
	NETADDR addr;
	TSendBuffer send_buffer;
};

struct context_data
{
	char bindaddr_str[NETADDR_MAXSTRSIZE];
	// Copies of the paths as they were when the context was created. lws keeps its
	// own copy, which only these still match after the config has been changed.
	char ssl_cert_path[IO_MAX_PATH_LENGTH];
	char ssl_key_path[IO_MAX_PATH_LENGTH];
	lws_context_creation_info creation_info;
	lws_context *context;
	std::map<NETADDR, per_session_data *> port_map;
	// Accepted connections from adoption until destruction. Unlike port_map, this
	// also covers connections still in the TLS/HTTP handshake phase, whose sockets
	// must be watched for the handshake to progress between select() timeouts.
	std::set<lws *> adopted_wsis;
	TRecvBuffer recv_buffer;
	int64_t accept_window_start;
	int accept_window_count;
};

// Accepting a connection completes its TLS handshake on the game loop's thread,
// before any DDNet-level limit can apply, so this is what keeps an
// unauthenticated peer from stalling the server's ticks by connecting in a loop.
// Far above what legitimate joins need, far below what a flood achieves.
static constexpr int WEBSOCKET_MAX_ACCEPTS_PER_SECOND = 50;
#if defined(LWS_WITH_PEER_LIMITS)
// Bounds concurrent connections per peer. Only available when lws was built with
// peer limits, so the accept rate limit above cannot rely on it.
static constexpr unsigned short WEBSOCKET_MAX_CONNECTIONS_PER_IP = 20;
#endif

// Client has main, dummy and contact connections with IPv4 and IPv6
static context_data contexts[3 * 2];
static std::map<lws_context *, context_data *> contexts_map;

static lws_context *websocket_context(int socket)
{
	dbg_assert(socket >= 0 && socket < (int)std::size(contexts), "socket index invalid: %d", socket);
	lws_context *context = contexts[socket].context;
	dbg_assert(context != nullptr, "socket context not initialized: %d", socket);
	return context;
}

static void receive_chunk(context_data *ctx_data, per_session_data *pss, const void *in, size_t len)
{
	websocket_chunk *chunk = ctx_data->recv_buffer.Allocate(len + sizeof(websocket_chunk));
	dbg_assert(chunk != nullptr, "failed to allocate websocket receive buffer chunk of size %" PRIzu, len);
	chunk->size = len;
	chunk->read = 0;
	chunk->addr = pss->addr;
	mem_copy(&chunk->data[0], in, len);
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

static int websocket_protocol_callback(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	per_session_data *pss = (per_session_data *)user;
	lws_context *context = lws_get_context(wsi);
	context_data *ctx_data = contexts_map[context];
	switch(reason)
	{
	case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
	{
		// Issued right after accept(), before the TLS handshake is started, and
		// returning non-zero closes the connection there. Rate limit it, because
		// everything past this point runs on the game loop's thread.
		const int64_t now = time_get();
		if(now - ctx_data->accept_window_start > time_freq())
		{
			ctx_data->accept_window_start = now;
			ctx_data->accept_window_count = 0;
		}
		ctx_data->accept_window_count++;
		if(ctx_data->accept_window_count > WEBSOCKET_MAX_ACCEPTS_PER_SECOND)
		{
			return 1;
		}
		return 0;
	}

	case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
		// Fires once the accepted connection has its socket attached. From here on
		// every death of the wsi goes through LWS_CALLBACK_WSI_DESTROY, which is
		// not guaranteed for wsis that failed adoption before this point.
		ctx_data->adopted_wsis.insert(wsi);
		return 0;

	case LWS_CALLBACK_ESTABLISHED:
		// Bit 0 of len marks a websocket running over an HTTP/2 stream (RFC 8441).
		// All streams of an HTTP/2 connection share its peer address, which is the
		// only identity the network layer knows a connection by, so they cannot be
		// told apart and are refused by returning non-zero, which closes them.
		if((len & 1) != 0)
		{
			return 1;
		}
		[[fallthrough]];
	case LWS_CALLBACK_WSI_CREATE:
	{
		if(pss == nullptr)
		{
			return 0;
		}
		sockaddr_storage peersockaddr;
		socklen_t peersockaddr_size = sizeof(peersockaddr);
		if(getpeername(lws_get_socket_fd(wsi), (sockaddr *)&peersockaddr, &peersockaddr_size) != 0)
		{
			log_warn("websockets", "Failed to determine peer address: %s", net_error_message().c_str());
			return 0;
		}
		NETADDR addr;
		sockaddr_to_netaddr_websocket((sockaddr *)&peersockaddr, peersockaddr_size, &addr);
		if(addr.type == NETTYPE_INVALID)
		{
			return 0;
		}

		pss->wsi = wsi;
		pss->addr = addr;
		pss->send_buffer.Init();
		ctx_data->port_map[addr] = pss;

		char addr_str[NETADDR_MAXSTRSIZE];
		net_addr_str(&addr, addr_str, sizeof(addr_str), true);
		log_trace("websockets", "Connection established with '%s'", addr_str);
		return 0;
	}

	case LWS_CALLBACK_CLIENT_CLOSED:
		[[fallthrough]];
	case LWS_CALLBACK_CLOSED:
	{
		char addr_str[NETADDR_MAXSTRSIZE];
		net_addr_str(&pss->addr, addr_str, sizeof(addr_str), true);
		log_trace("websockets", "Connection closed with '%s'", addr_str);

		static const unsigned char CLOSE_PACKET[] = {0x10, 0x0e, 0x00, 0x04};
		receive_chunk(ctx_data, pss, &CLOSE_PACKET, sizeof(CLOSE_PACKET));
		return 0;
	}

	case LWS_CALLBACK_WSI_DESTROY:
	{
		ctx_data->adopted_wsis.erase(wsi);
		if(pss == nullptr)
		{
			return 0;
		}
		pss->wsi = nullptr;
		ctx_data->port_map.erase(pss->addr);
		return 0;
	}

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		[[fallthrough]];
	case LWS_CALLBACK_SERVER_WRITEABLE:
	{
		websocket_chunk *chunk = pss->send_buffer.First();
		if(chunk == nullptr)
		{
			return 0;
		}

		int chunk_len = chunk->size - chunk->read;
		int n = lws_write(wsi, &chunk->data[LWS_SEND_BUFFER_PRE_PADDING + chunk->read], chunk->size - chunk->read, LWS_WRITE_BINARY);
		if(n < 0)
		{
			return 1;
		}

		if(n < chunk_len)
		{
			chunk->read += n;
			lws_callback_on_writable(wsi);
			return 0;
		}

		pss->send_buffer.PopFirst();
		lws_callback_on_writable(wsi);
		return 0;
	}

	case LWS_CALLBACK_CLIENT_RECEIVE:
		[[fallthrough]];
	case LWS_CALLBACK_RECEIVE:
		receive_chunk(ctx_data, pss, in, len);
		return 0;

	default:
		return 0;
	}
}

static const lws_protocols protocols[] = {
	{"binary", websocket_protocol_callback, sizeof(per_session_data)},
	{"base64", websocket_protocol_callback, sizeof(per_session_data)},
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

void websocket_reload_certs()
{
#if defined(LWS_WITH_TLS)
	for(context_data &ctx_data : contexts)
	{
		if(ctx_data.context == nullptr || ctx_data.creation_info.ssl_cert_filepath == nullptr)
		{
			continue;
		}
		if(str_comp(ctx_data.ssl_cert_path, g_Config.m_SvWebsocketCert) != 0 || str_comp(ctx_data.ssl_key_path, g_Config.m_SvWebsocketKey) != 0)
		{
			// lws can only reload a certificate under the path it was given at startup,
			// so changing the config and reloading would silently keep the old one.
			log_warn("websockets", "Cannot reload certificate from a different path than '%s', restart the server instead", ctx_data.ssl_cert_path);
			continue;
		}
		log_info("websockets", "Reloading certificate '%s'", ctx_data.ssl_cert_path);
		lws_tls_cert_updated(ctx_data.context, ctx_data.ssl_cert_path, ctx_data.ssl_key_path, nullptr, 0, nullptr, 0);
	}
#else
	log_error("websockets", "Cannot reload certificate: libwebsockets was built without TLS support");
#endif
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
#if defined(LWS_WITH_TLS)
	// Only offer HTTP/1.1. Browsers that negotiate HTTP/2 run websockets over it as
	// streams (RFC 8441), which all share the peer address of their connection and
	// therefore cannot be told apart. ALPN is a TLS extension, and the field only
	// exists when lws was built with TLS support.
	ctx_data->creation_info.alpn = "http/1.1";
#endif
#if defined(LWS_WITH_PEER_LIMITS)
	ctx_data->creation_info.ip_limit_wsi = WEBSOCKET_MAX_CONNECTIONS_PER_IP;
#endif
	if(g_Config.m_SvWebsocketCert[0] != '\0' || g_Config.m_SvWebsocketKey[0] != '\0')
	{
#if defined(LWS_WITH_TLS)
		if(g_Config.m_SvWebsocketCert[0] == '\0' || g_Config.m_SvWebsocketKey[0] == '\0')
		{
			log_error("websockets", "sv_websocket_cert and sv_websocket_key must both be set to serve wss");
			return -1;
		}
		str_copy(ctx_data->ssl_cert_path, g_Config.m_SvWebsocketCert);
		str_copy(ctx_data->ssl_key_path, g_Config.m_SvWebsocketKey);
		ctx_data->creation_info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		ctx_data->creation_info.ssl_cert_filepath = ctx_data->ssl_cert_path;
		ctx_data->creation_info.ssl_private_key_filepath = ctx_data->ssl_key_path;
#else
		// lws without TLS support ignores the certificate config and would
		// silently serve unencrypted websockets instead of wss.
		log_error("websockets", "Cannot serve wss: libwebsockets was built without TLS support");
		return -1;
#endif
	}
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
	return first_free;
}

void websocket_destroy(int socket)
{
	lws_context *context = websocket_context(socket);
	lws_context_destroy(context);
	contexts_map.erase(context);
	contexts[socket].context = nullptr;
}

int websocket_recv(int socket, unsigned char *data, size_t maxsize, NETADDR *addr)
{
	lws_context *context = websocket_context(socket);
	const int service_result = lws_service(context, -1);
	if(service_result < 0)
	{
		return service_result;
	}

	context_data *ctx_data = contexts_map[context];
	websocket_chunk *chunk = ctx_data->recv_buffer.First();
	if(chunk == nullptr)
	{
		return 0;
	}

	if(maxsize >= chunk->size - chunk->read)
	{
		const int len = chunk->size - chunk->read;
		mem_copy(data, &chunk->data[chunk->read], len);
		*addr = chunk->addr;
		ctx_data->recv_buffer.PopFirst();
		return len;
	}
	else
	{
		mem_copy(data, &chunk->data[chunk->read], maxsize);
		*addr = chunk->addr;
		chunk->read += maxsize;
		return maxsize;
	}
}

int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr)
{
	lws_context *context = websocket_context(socket);
	context_data *ctx_data = contexts_map[context];
	per_session_data *pss = ctx_data->port_map[*addr];
	if(pss == nullptr)
	{
		char addr_str[NETADDR_MAXSTRSIZE];
		net_addr_str(addr, addr_str, sizeof(addr_str), false);
		lws_client_connect_info ccinfo = {};
		ccinfo.context = context;
		ccinfo.address = addr_str;
		ccinfo.port = addr->port;
		ccinfo.protocol = protocols[0].name;
		lws *wsi = lws_client_connect_via_info(&ccinfo);
		if(wsi == nullptr)
		{
			return -1;
		}
		lws_service(context, -1);
		pss = ctx_data->port_map[*addr];
		if(pss == nullptr)
		{
			return -1;
		}
	}

	const size_t chunk_size = size + sizeof(websocket_chunk) + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING;
	websocket_chunk *chunk = pss->send_buffer.Allocate(chunk_size);
	dbg_assert(chunk != nullptr, "failed to allocate websocket send buffer chunk of size %" PRIzu, size);
	mem_zero(chunk, chunk_size);
	chunk->size = size;
	chunk->read = 0;
	chunk->addr = pss->addr;
	mem_copy(&chunk->data[LWS_SEND_BUFFER_PRE_PADDING], data, size);
	lws_callback_on_writable(pss->wsi);
	lws_service(context, -1);
	return size;
}

int websocket_fd_set(int socket, fd_set *set)
{
	lws_context *context = websocket_context(socket);
	lws_service(context, -1);

	context_data *ctx_data = contexts_map[context];
	int max = 0;
	for(lws *wsi : ctx_data->adopted_wsis)
	{
		const int fd = lws_get_socket_fd(wsi);
		if(fd < 0)
		{
			continue;
		}
		max = std::max(fd, max);
		FD_SET(fd, set);
	}
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
	lws_service(context, -1);

	context_data *ctx_data = contexts_map[context];
	if(ctx_data->recv_buffer.First() != nullptr)
	{
		// Packets already consumed by lws_service are no longer readable on the
		// socket, so select cannot see them and they would never be processed.
		return 1;
	}
	for(lws *wsi : ctx_data->adopted_wsis)
	{
		const int fd = lws_get_socket_fd(wsi);
		if(fd >= 0 && FD_ISSET(fd, set))
		{
			return 1;
		}
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
