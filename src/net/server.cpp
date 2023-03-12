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

unsigned char IDENTITY[] = {
	0x5c, 0xf2, 0xa4, 0xf0, 0xed, 0x3d, 0xc8, 0x5b, 0x3f, 0x4b, 0xfa, 0x5c,
	0xa9, 0x7b, 0x8a, 0xde, 0xaf, 0x0d, 0x5e, 0xc1, 0x65, 0x17, 0x9a, 0xf8,
	0x05, 0xa3, 0xf7, 0x75, 0x1d, 0xd8, 0xac, 0x47
};

int main(int argc, const char **argv)
{
	DdnetNet *net = nullptr;
	H(ddnet_net_new(&net));
	H(ddnet_net_set_bindaddr(net, "0.0.0.0:4433", 12));
	H(ddnet_net_set_identity(net, &IDENTITY));
	H(ddnet_net_set_accept_incoming_connections(net, true));
	H(ddnet_net_open(net));

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
			break;
		case DDNET_NET_EV_CHUNK:
			{
				bool nonvital = ddnet_net_ev_chunk_is_unreliable(&ev);
				if(nonvital) {
					H(ddnet_net_close(net, idx, "blib?", 5));
				}
			}
			break;
		}
	}
}
