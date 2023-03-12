#include "net.h"

#include <stdio.h>
#include <string.h>

void print_error_and_quit(DdnetNet *net)
{
	printf("%s\n", ddnet_net_error(net));
	ddnet_net_free(net);
	exit(1);
}

#define H(expr) \
	do \
	{ \
		if(expr) \
		{ \
			print_error_and_quit(net); \
		} \
	} while(0)

int main(int argc, const char **argv)
{
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s <URL>\n", argc >= 1 ? argv[0] : "client");
		return 1;
	}

	DdnetNet *net = nullptr;
	H(ddnet_net_new(&net));
	H(ddnet_net_open(net));
	uint64_t conn_idx;
	H(ddnet_net_connect(net, argv[1], strlen(argv[1]), &conn_idx));

	uint8_t buffer[4096];
	uint64_t idx;
	DdnetNetEvent ev;

	while(1)
	{
		H(ddnet_net_recv(net, buffer, sizeof(buffer), &idx, &ev));
		uint64_t ev_kind = ddnet_net_ev_kind(&ev);
		switch(ev_kind)
		{
		case DDNET_NET_EV_NONE:
			H(ddnet_net_wait(net));
			continue;
		case DDNET_NET_EV_CONNECT:
			H(ddnet_net_send_chunk(net, idx, (uint8_t *)"blab", 4, true));
			H(ddnet_net_send_chunk(net, idx, (uint8_t *)"blob", 4, false));
			H(ddnet_net_send_chunk(net, idx, (uint8_t *)"blub", 4, false));
			break;
		case DDNET_NET_EV_DISCONNECT:
			{
				size_t len = ddnet_net_ev_disconnect_reason_len(&ev);
				if(len > 0)
				{
					if(len == sizeof(buffer))
					{
						len = sizeof(buffer) - 1;
					}
					buffer[len] = 0;
					printf("disconnect: %s\n", buffer);
				}
			}
			return 0;
		}
	}
}
