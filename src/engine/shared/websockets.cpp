#if defined(CONF_WEBSOCKETS)

#include "websockets.h"

#include <cstdlib>
#include <map>
#include <string>

#include "protocol.h"
#include "ringbuffer.h"

#include <base/log.h>
#include <base/system.h>

#if defined(CONF_FAMILY_UNIX)
#include <arpa/inet.h>
#elif defined(CONF_FAMILY_WINDOWS)
#include <ws2tcpip.h>
#endif
#include <libwebsockets.h>

// ddnet client opens two connections for whatever reason
typedef CStaticRingBuffer<unsigned char, (MAX_CLIENTS * 2) * 4 * 1024,
	CRingBufferBase::FLAG_RECYCLE>
	TRecvBuffer;
typedef CStaticRingBuffer<unsigned char, 4 * 1024,
	CRingBufferBase::FLAG_RECYCLE>
	TSendBuffer;

struct websocket_chunk
{
	size_t size;
	size_t read;
	sockaddr_in addr;
	unsigned char data[0];
};

struct per_session_data
{
	struct lws *wsi;
	std::string addr_str;
	sockaddr_in addr;
	TSendBuffer send_buffer;
};

struct context_data
{
	lws_context_creation_info creation_info;
	lws_context *context;
	std::map<std::string, per_session_data *> port_map;
	TRecvBuffer recv_buffer;
};

// not sure why would anyone need more than one but well...
static context_data contexts[4];
static std::map<struct lws_context *, context_data *> contexts_map;

static lws_context *websocket_context(int socket)
{
	dbg_assert(socket >= 0 && socket < (int)std::size(contexts), "socket index invalid: %d", socket);
	lws_context *context = contexts[socket].context;
	dbg_assert(context != nullptr, "socket context not initialized: %d", socket);
	return context;
}

static int receive_chunk(context_data *ctx_data, struct per_session_data *pss,
	void *in, size_t len)
{
	websocket_chunk *chunk = (websocket_chunk *)ctx_data->recv_buffer.Allocate(
		len + sizeof(websocket_chunk));
	if(chunk == 0)
		return 1;
	chunk->size = len;
	chunk->read = 0;
	mem_copy(&chunk->addr, &pss->addr, sizeof(sockaddr_in));
	mem_copy(&chunk->data[0], in, len);
	return 0;
}

static int websocket_protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len)
{
	struct per_session_data *pss = (struct per_session_data *)user;
	lws_context *context = lws_get_context(wsi);
	context_data *ctx_data = contexts_map[context];
	switch(reason)
	{
	case LWS_CALLBACK_WSI_CREATE:
		if(pss == nullptr)
		{
			return 0;
		}
		[[fallthrough]];
	case LWS_CALLBACK_ESTABLISHED:
	{
		pss->wsi = wsi;
		int fd = lws_get_socket_fd(wsi);
		socklen_t addr_size = sizeof(pss->addr);
		getpeername(fd, (struct sockaddr *)&pss->addr, &addr_size);
		int orig_port = ntohs(pss->addr.sin_port);
		pss->send_buffer.Init();

		char addr_str[NETADDR_MAXSTRSIZE];
		int ip_uint32 = pss->addr.sin_addr.s_addr;
		str_format(addr_str, sizeof(addr_str), "%d.%d.%d.%d:%d", (ip_uint32)&0xff, (ip_uint32 >> 8) & 0xff, (ip_uint32 >> 16) & 0xff, (ip_uint32 >> 24) & 0xff, orig_port);

		log_trace("websockets", "Connection established with '%s'", addr_str);

		pss->addr_str = addr_str;
		ctx_data->port_map[pss->addr_str] = pss;
	}
	break;

	case LWS_CALLBACK_CLOSED:
	{
		log_trace("websockets", "Connection closed with '%s'", pss->addr_str.c_str());
		if(!pss->addr_str.empty())
		{
			unsigned char close_packet[] = {0x10, 0x0e, 0x00, 0x04};
			receive_chunk(ctx_data, pss, &close_packet, sizeof(close_packet));
			pss->wsi = 0;
			ctx_data->port_map.erase(pss->addr_str);
		}
	}
	break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		[[fallthrough]];
	case LWS_CALLBACK_SERVER_WRITEABLE:
	{
		websocket_chunk *chunk = (websocket_chunk *)pss->send_buffer.First();
		if(chunk == nullptr)
			break;
		int chunk_len = chunk->size - chunk->read;
		int n =
			lws_write(wsi, &chunk->data[LWS_SEND_BUFFER_PRE_PADDING + chunk->read],
				chunk->size - chunk->read, LWS_WRITE_BINARY);
		if(n < 0)
			return 1;
		if(n < chunk_len)
		{
			chunk->read += n;
			lws_callback_on_writable(wsi);
			break;
		}
		pss->send_buffer.PopFirst();
		lws_callback_on_writable(wsi);
	}
	break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		[[fallthrough]];
	case LWS_CALLBACK_RECEIVE:
		if(pss->addr_str.empty())
			return -1;
		if(receive_chunk(ctx_data, pss, in, len))
			return 1;
		break;

	default:
		break;
	}

	return 0;
}

static struct lws_protocols protocols[] = {
	{
		"binary", /* name */
		websocket_protocol_callback, /* callback */
		sizeof(struct per_session_data) /* per_session_data_size */
	},
	{
		nullptr, nullptr, 0 /* End of list */
	}};

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
		dbg_assert(false, "invalid log level: %d", level);
		dbg_break();
	}
}

static void websocket_log_callback(int level, const char *line)
{
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

int websocket_create(const char *addr, int port)
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
	ctx_data->creation_info.port = port;
	ctx_data->creation_info.iface = addr;
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
	return first_free;
}

int websocket_destroy(int socket)
{
	lws_context *context = websocket_context(socket);
	lws_context_destroy(context);
	contexts_map.erase(context);
	contexts[socket].context = nullptr;
	return 0;
}

int websocket_recv(int socket, unsigned char *data, size_t maxsize,
	struct sockaddr_in *sockaddrbuf, size_t fromLen)
{
	lws_context *context = websocket_context(socket);
	int n = lws_service(context, -1);
	if(n < 0)
		return n;
	context_data *ctx_data = contexts_map[context];
	websocket_chunk *chunk = (websocket_chunk *)ctx_data->recv_buffer.First();
	if(chunk == 0)
		return 0;
	if(maxsize >= chunk->size - chunk->read)
	{
		int len = chunk->size - chunk->read;
		mem_copy(data, &chunk->data[chunk->read], len);
		mem_copy(sockaddrbuf, &chunk->addr, fromLen);
		ctx_data->recv_buffer.PopFirst();
		return len;
	}
	else
	{
		mem_copy(data, &chunk->data[chunk->read], maxsize);
		mem_copy(sockaddrbuf, &chunk->addr, fromLen);
		chunk->read += maxsize;
		return maxsize;
	}
}

int websocket_send(int socket, const unsigned char *data, size_t size,
	const char *addr_str, int port)
{
	lws_context *context = websocket_context(socket);
	context_data *ctx_data = contexts_map[context];
	std::string addr_str_with_port = std::string(addr_str) + ":" + std::to_string(port);
	struct per_session_data *pss = ctx_data->port_map[addr_str_with_port];
	if(pss == nullptr)
	{
		struct lws_client_connect_info ccinfo = {0};
		ccinfo.context = context;
		ccinfo.address = addr_str;
		ccinfo.port = port;
		ccinfo.protocol = protocols[0].name;
		lws *wsi = lws_client_connect_via_info(&ccinfo);
		if(wsi == nullptr)
		{
			return -1;
		}
		lws_service(context, -1);
		pss = ctx_data->port_map[addr_str_with_port];
		if(pss == nullptr)
		{
			return -1;
		}
	}
	websocket_chunk *chunk = (websocket_chunk *)pss->send_buffer.Allocate(
		size + sizeof(websocket_chunk) + LWS_SEND_BUFFER_PRE_PADDING +
		LWS_SEND_BUFFER_POST_PADDING);
	if(chunk == nullptr)
		return -1;
	chunk->size = size;
	chunk->read = 0;
	mem_copy(&chunk->addr, &pss->addr, sizeof(sockaddr_in));
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
	for(const auto &[_, pss] : ctx_data->port_map)
	{
		if(pss == nullptr)
			continue;
		int fd = lws_get_socket_fd(pss->wsi);
		if(fd > max)
			max = fd;
		FD_SET(fd, set);
	}
	return max;
}

int websocket_fd_get(int socket, fd_set *set)
{
	lws_context *context = websocket_context(socket);
	lws_service(context, -1);
	context_data *ctx_data = contexts_map[context];
	for(const auto &[_, pss] : ctx_data->port_map)
	{
		if(pss == nullptr)
			continue;
		if(FD_ISSET(lws_get_socket_fd(pss->wsi), set))
			return 1;
	}
	return 0;
}

#endif
