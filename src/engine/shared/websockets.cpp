#if defined(CONF_WEBSOCKETS)

#include <cstdlib>
#include <map>
#include <string>

#include "protocol.h"
#include "ringbuffer.h"
#include <base/system.h>
#if defined(CONF_FAMILY_UNIX)
#include <arpa/inet.h>
#elif defined(CONF_FAMILY_WINDOWS)
#include <ws2tcpip.h>
#endif
#include <libwebsockets.h>

#include "websockets.h"

// not sure why would anyone need more than one but well...
#define WS_CONTEXTS 4
// ddnet client opens two connections for whatever reason
#define WS_CLIENTS (MAX_CLIENTS * 2)

typedef CStaticRingBuffer<unsigned char, WS_CLIENTS * 4 * 1024,
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
	lws_context *context;
	std::map<std::string, per_session_data *> port_map;
	TRecvBuffer recv_buffer;
};

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

static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len)
{
	struct per_session_data *pss = (struct per_session_data *)user;
	lws_context *context = lws_get_context(wsi);
	context_data *ctx_data = (context_data *)lws_context_user(context);
	switch(reason)
	{
	case LWS_CALLBACK_WSI_CREATE:
		if(pss == NULL)
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

		dbg_msg("websockets", "connection established with %s", addr_str);

		std::string addr_str_final;
		addr_str_final.append(addr_str);

		pss->addr_str = addr_str_final;
		ctx_data->port_map[pss->addr_str] = pss;
	}
	break;

	case LWS_CALLBACK_CLOSED:
	{
		dbg_msg("websockets", "connection with addr string %s closed", pss->addr_str.c_str());
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
		if(chunk == NULL)
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
		websocket_callback, /* callback */
		sizeof(struct per_session_data) /* per_session_data_size */
	},
	{
		NULL, NULL, 0 /* End of list */
	}};

static context_data contexts[WS_CONTEXTS];

int websocket_create(const char *addr, int port)
{
	struct lws_context_creation_info info;
	mem_zero(&info, sizeof(info));
	info.port = port;
	info.iface = addr;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	// find free context
	int first_free = -1;
	for(int i = 0; i < WS_CONTEXTS; i++)
	{
		if(contexts[i].context == NULL)
		{
			first_free = i;
			break;
		}
	}
	if(first_free == -1)
		return -1;

	context_data *ctx_data = &contexts[first_free];
	info.user = (void *)ctx_data;

	ctx_data->context = lws_create_context(&info);
	if(ctx_data->context == NULL)
	{
		return -1;
	}
	ctx_data->recv_buffer.Init();
	return first_free;
}

int websocket_destroy(int socket)
{
	lws_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	lws_context_destroy(context);
	contexts[socket].context = NULL;
	return 0;
}

int websocket_recv(int socket, unsigned char *data, size_t maxsize,
	struct sockaddr_in *sockaddrbuf, size_t fromLen)
{
	lws_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	int n = lws_service(context, -1);
	if(n < 0)
		return n;
	context_data *ctx_data = (context_data *)lws_context_user(context);
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
	lws_context *context = contexts[socket].context;
	if(context == NULL)
	{
		return -1;
	}
	context_data *ctx_data = (context_data *)lws_context_user(context);
	char aBuf[100];
	snprintf(aBuf, sizeof(aBuf), "%s:%d", addr_str, port);
	std::string addr_str_with_port = std::string(aBuf);
	struct per_session_data *pss = ctx_data->port_map[addr_str_with_port];
	if(pss == NULL)
	{
		struct lws_client_connect_info ccinfo = {0};
		ccinfo.context = context;
		ccinfo.address = addr_str;
		ccinfo.port = port;
		ccinfo.protocol = protocols[0].name;
		lws *wsi = lws_client_connect_via_info(&ccinfo);
		if(wsi == NULL)
		{
			return -1;
		}
		lws_service(context, -1);
		pss = ctx_data->port_map[addr_str_with_port];
		if(pss == NULL)
		{
			return -1;
		}
	}
	websocket_chunk *chunk = (websocket_chunk *)pss->send_buffer.Allocate(
		size + sizeof(websocket_chunk) + LWS_SEND_BUFFER_PRE_PADDING +
		LWS_SEND_BUFFER_POST_PADDING);
	if(chunk == NULL)
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
	lws_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	lws_service(context, -1);
	context_data *ctx_data = (context_data *)lws_context_user(context);
	int max = 0;
	for(auto const &x : ctx_data->port_map)
	{
		if(x.second == NULL)
			continue;
		int fd = lws_get_socket_fd(x.second->wsi);
		if(fd > max)
			max = fd;
		FD_SET(fd, set);
	}
	return max;
}

#endif
