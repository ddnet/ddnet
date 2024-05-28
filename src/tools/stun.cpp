#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/stun.h>

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);

	log_set_global_logger_default();
	if(secure_random_init() != 0)
	{
		log_error("stun", "could not initialize secure RNG");
		return -1;
	}

	if(argc < 2)
	{
		log_info("stun", "usage: %s <STUN ADDRESS>", argc > 0 ? argv[0] : "stun");
		return 1;
	}
	NETADDR Addr;
	if(net_addr_from_str(&Addr, argv[1]))
	{
		log_error("stun", "couldn't parse address");
		return 2;
	}

	net_init();
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_ALL;
	NETSOCKET Socket = net_udp_create(BindAddr);
	if(net_socket_type(Socket) == NETTYPE_INVALID)
	{
		log_error("stun", "couldn't open udp socket");
		return 2;
	}

	CStunData Stun;
	unsigned char aRequest[32];
	int RequestSize = StunMessagePrepare(aRequest, sizeof(aRequest), &Stun);
	if(net_udp_send(Socket, &Addr, aRequest, RequestSize) == -1)
	{
		log_error("stun", "failed sending stun request");
		return 2;
	}

	NETADDR ResponseAddr;
	unsigned char *pResponse;
	while(true)
	{
		if(!net_socket_read_wait(Socket, 1000000))
		{
			log_error("stun", "no udp message received from server until timeout");
			return 3;
		}
		int ResponseSize = net_udp_recv(Socket, &ResponseAddr, &pResponse);
		if(ResponseSize == -1)
		{
			log_error("stun", "failed receiving udp message");
			return 2;
		}
		else if(ResponseSize == 0)
		{
			continue;
		}
		if(net_addr_comp(&Addr, &ResponseAddr) != 0)
		{
			char aResponseAddr[NETADDR_MAXSTRSIZE];
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&ResponseAddr, aResponseAddr, sizeof(aResponseAddr), true);
			net_addr_str(&Addr, aAddr, sizeof(aAddr), true);
			log_debug("stun", "got message from %s while expecting one from %s", aResponseAddr, aAddr);
			continue;
		}
		bool Success;
		NETADDR StunAddr;
		if(StunMessageParse(pResponse, ResponseSize, &Stun, &Success, &StunAddr))
		{
			log_debug("stun", "received message from stun server that was not understood");
			continue;
		}
		if(Success)
		{
			char aStunAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&StunAddr, aStunAddr, sizeof(aStunAddr), 1);
			log_info("stun", "public ip address: %s", aStunAddr);
			break;
		}
		else
		{
			log_info("stun", "error response from stun server");
			break;
		}
	}
}
