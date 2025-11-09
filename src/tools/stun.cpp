#include <base/logger.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/os.h>

#include <engine/shared/stun.h>

#include <chrono>

using namespace std::chrono_literals;

static constexpr const char *const TOOL_NAME = "stun";

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);

	log_set_global_logger_default();

	if(argc < 2)
	{
		log_error(TOOL_NAME, "Usage: %s <STUN ADDRESS>", argc > 0 ? argv[0] : TOOL_NAME);
		return 1;
	}
	NETADDR Addr;
	if(net_addr_from_str(&Addr, argv[1]))
	{
		log_error(TOOL_NAME, "couldn't parse address");
		return 2;
	}

	net_init();
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_ALL;
	NETSOCKET Socket = net_udp_create(BindAddr);
	if(net_socket_type(Socket) == NETTYPE_INVALID)
	{
		log_error(TOOL_NAME, "couldn't open udp socket");
		return 2;
	}

	CStunData Stun;
	unsigned char aRequest[32];
	int RequestSize = StunMessagePrepare(aRequest, sizeof(aRequest), &Stun);
	if(net_udp_send(Socket, &Addr, aRequest, RequestSize) == -1)
	{
		log_error(TOOL_NAME, "failed sending stun request");
		return 2;
	}

	NETADDR ResponseAddr;
	unsigned char *pResponse;
	while(true)
	{
		if(!net_socket_read_wait(Socket, 1s))
		{
			log_error(TOOL_NAME, "no udp message received from server until timeout");
			return 3;
		}
		int ResponseSize = net_udp_recv(Socket, &ResponseAddr, &pResponse);
		if(ResponseSize == -1)
		{
			log_error(TOOL_NAME, "failed receiving udp message");
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
			log_debug(TOOL_NAME, "got message from %s while expecting one from %s", aResponseAddr, aAddr);
			continue;
		}
		bool Success;
		NETADDR StunAddr;
		if(StunMessageParse(pResponse, ResponseSize, &Stun, &Success, &StunAddr))
		{
			log_debug(TOOL_NAME, "received message from stun server that was not understood");
			continue;
		}
		if(Success)
		{
			char aStunAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&StunAddr, aStunAddr, sizeof(aStunAddr), true);
			log_info(TOOL_NAME, "public ip address: %s", aStunAddr);
			break;
		}
		else
		{
			log_info(TOOL_NAME, "error response from stun server");
			break;
		}
	}
}
