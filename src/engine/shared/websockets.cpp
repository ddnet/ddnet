#if defined(CONF_WEBSOCKETS)

#include <string.h>
#include <stdlib.h>

#include "engine/external/libwebsockets/libwebsockets.h"
#include "base/system.h"
#include "protocol.h"
#include "ringbuffer.h"
#include <arpa/inet.h>

extern "C" {

#include "websockets.h"

// not sure why would anyone need more than one but well...
#define WS_CONTEXTS 4
// ddnet client opens two connections for whatever reason
#define WS_CLIENTS MAX_CLIENTS*2

typedef TStaticRingBuffer<unsigned char, WS_CLIENTS * 4 * 1024, CRingBufferBase::FLAG_RECYCLE> TRecvBuffer;
typedef TStaticRingBuffer<unsigned char, 4 * 1024, CRingBufferBase::FLAG_RECYCLE> TSendBuffer;

typedef struct
{
	size_t size;
	size_t read;
	sockaddr_in addr;
	unsigned char data[0];
} websocket_chunk;

struct per_session_data
{
	struct libwebsocket *wsi;
	int port;
	sockaddr_in addr;
	TSendBuffer send_buffer;
};

struct context_data
{
	libwebsocket_context *context;
	per_session_data *port_map[WS_CLIENTS];
	TRecvBuffer recv_buffer;
	int last_used_port;
};

static int receive_chunk(context_data *ctx_data, struct per_session_data *pss, void *in, size_t len)
{
	websocket_chunk *chunk = (websocket_chunk *)ctx_data->recv_buffer.Allocate(len + sizeof(websocket_chunk));
	if(chunk == 0)
		return 1;
	chunk->size = len;
	chunk->read = 0;
	memcpy(&chunk->addr, &pss->addr, sizeof(sockaddr_in));
	memcpy(&chunk->data[0], in, len);
	return 0;
}

static int websocket_callback(struct libwebsocket_context *context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{
	struct per_session_data *pss = (struct per_session_data *)user;
	context_data *ctx_data = (context_data *)libwebsocket_context_user(context);

	switch(reason)
	{

	case LWS_CALLBACK_ESTABLISHED:
	{
		int port = -1;
		for(int i = 0; i < WS_CLIENTS; i++)
		{
			int j = (ctx_data->last_used_port + i + 1) % WS_CLIENTS;
			if(ctx_data->port_map[j] == NULL)
			{
				port = j;
				break;
			}
		}
		if(port == -1)
		{
			dbg_msg("websockets", "no free ports, dropping");
			pss->port = -1;
			return -1;
		}
		ctx_data->last_used_port = port;
		pss->wsi = wsi;
		int fd = libwebsocket_get_socket_fd(wsi);
		socklen_t addr_size = sizeof(pss->addr);
		getpeername(fd, (struct sockaddr *)&pss->addr, &addr_size);
		int orig_port = ntohs(pss->addr.sin_port);
		pss->addr.sin_port = htons(port);
		pss->send_buffer.Init();
		pss->port = port;
		ctx_data->port_map[port] = pss;
		char addr_str[NETADDR_MAXSTRSIZE];
		inet_ntop(AF_INET, &pss->addr.sin_addr, addr_str, sizeof(addr_str));
		dbg_msg("websockets", "connection established with %s:%d , assigned fake port %d", addr_str, orig_port, port);
	}
	break;

	case LWS_CALLBACK_CLOSED:
	{
		dbg_msg("websockets", "connection with fake port %d closed", pss->port);
		if (pss->port > -1) {
			unsigned char close_packet[] = { 0x10, 0x0e, 0x00, 0x04 };
			receive_chunk(ctx_data, pss, &close_packet, sizeof(close_packet));
			pss->wsi = 0;
			ctx_data->port_map[pss->port] = NULL;
		}
	}
	break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
	{
		websocket_chunk *chunk = (websocket_chunk *)pss->send_buffer.First();
		if(chunk == NULL)
			break;
		int len = chunk->size - chunk->read;
		int n = libwebsocket_write(wsi, &chunk->data[LWS_SEND_BUFFER_PRE_PADDING + chunk->read], chunk->size - chunk->read, LWS_WRITE_BINARY);
		if(n < 0)
			return 1;
		if(n < len)
		{
			chunk->read += n;
			libwebsocket_callback_on_writable(context, wsi);
			break;
		}
		pss->send_buffer.PopFirst();
		libwebsocket_callback_on_writable(context, wsi);
	}
	break;

	case LWS_CALLBACK_RECEIVE:
		if(pss->port == -1)
			return -1;
		if(!receive_chunk(ctx_data, pss, in, len))
			return 1;
		break;

	default:
		break;
	}

	return 0;
}

static struct libwebsocket_protocols protocols[] = { {
		                                               "binary", /* name */
		                                               websocket_callback, /* callback */
		                                               sizeof(struct per_session_data) /* per_session_data_size */
		                                             },
	                                                 {
		                                               NULL, NULL, 0 /* End of list */
		                                             } };

static context_data contexts[WS_CONTEXTS];

int websocket_create(const char *addr, int port)
{
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
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

	ctx_data->context = libwebsocket_create_context(&info);
	if(ctx_data->context == NULL)
	{
		return -1;
	}
	memset(ctx_data->port_map, 0, sizeof(ctx_data->port_map));
	ctx_data->recv_buffer.Init();
	ctx_data->last_used_port = 0;
	return first_free;
}

int websocket_destroy(int socket)
{
	libwebsocket_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	libwebsocket_context_destroy(context);
	contexts[socket].context = NULL;
	return 0;
}

int websocket_recv(int socket, unsigned char *data, size_t maxsize, struct sockaddr_in *sockaddrbuf, size_t fromLen)
{
	libwebsocket_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	int n = libwebsocket_service(context, 0);
	if(n < 0)
		return n;
	context_data *ctx_data = (context_data *)libwebsocket_context_user(context);
	websocket_chunk *chunk = (websocket_chunk *)ctx_data->recv_buffer.First();
	if(chunk == 0)
		return 0;
	if(maxsize >= chunk->size - chunk->read)
	{
		int len = chunk->size - chunk->read;
		memcpy(data, &chunk->data[chunk->read], len);
		memcpy(sockaddrbuf, &chunk->addr, fromLen);
		ctx_data->recv_buffer.PopFirst();
		return len;
	}
	else
	{
		memcpy(data, &chunk->data[chunk->read], maxsize);
		memcpy(sockaddrbuf, &chunk->addr, fromLen);
		chunk->read += maxsize;
		return maxsize;
	}
}

int websocket_send(int socket, const unsigned char *data, size_t size, int port)
{
	libwebsocket_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	context_data *ctx_data = (context_data *)libwebsocket_context_user(context);
	struct per_session_data *pss = ctx_data->port_map[port];
	if(pss == NULL)
		return -1;
	websocket_chunk *chunk = (websocket_chunk *)pss->send_buffer.Allocate(size + sizeof(websocket_chunk) + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING);
	if(chunk == NULL)
		return -1;
	chunk->size = size;
	chunk->read = 0;
	memcpy(&chunk->addr, &pss->addr, sizeof(sockaddr_in));
	memcpy(&chunk->data[LWS_SEND_BUFFER_PRE_PADDING], data, size);
	libwebsocket_callback_on_writable(context, pss->wsi);
	return size;
}

int websocket_fd_set(int socket, fd_set *set)
{
	libwebsocket_context *context = contexts[socket].context;
	if(context == NULL)
		return -1;
	context_data *ctx_data = (context_data *)libwebsocket_context_user(context);
	int max = 0;
	for(int i = 0; i < WS_CLIENTS; i++)
	{
		per_session_data *pss = ctx_data->port_map[i];
		if(pss == NULL)
			continue;
		int fd = libwebsocket_get_socket_fd(pss->wsi);
		if(fd > max)
			max = fd;
		FD_SET(fd, set);
	}
	return max;
}
}
#endif
