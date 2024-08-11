#include <base/logger.h>
#include <base/system.h>

#include <engine/shared/masterserver.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>

#include <chrono>

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);

	log_set_global_logger_default();
	if(secure_random_init() != 0)
	{
		log_error("twping", "could not initialize secure RNG");
		return -1;
	}

	net_init();
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_ALL;

	CNetClient NetClient;
	NetClient.Open(BindAddr);

	if(argc != 2)
	{
		log_error("twping", "usage: %s server[:port] (default port: 8303)", argv[0]);
		return -1;
	}

	NETADDR Addr;
	if(net_host_lookup(argv[1], &Addr, NETTYPE_ALL))
	{
		log_error("twping", "host lookup failed");
		return -1;
	}

	if(Addr.port == 0)
		Addr.port = 8303;

	const int CurToken = rand() % 256;
	unsigned char aBuffer[sizeof(SERVERBROWSE_GETINFO) + 1];
	mem_copy(aBuffer, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO));
	aBuffer[sizeof(SERVERBROWSE_GETINFO)] = CurToken;

	CNetChunk Packet;
	Packet.m_ClientId = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(aBuffer);
	Packet.m_pData = aBuffer;

	NetClient.Send(&Packet);

	const int64_t StartTime = time_get();

	using namespace std::chrono_literals;
	const std::chrono::nanoseconds Timeout = std::chrono::nanoseconds(1s);
	net_socket_read_wait(NetClient.m_Socket, Timeout);

	NetClient.Update();

	SECURITY_TOKEN ResponseToken;
	while(NetClient.Recv(&Packet, &ResponseToken, false))
	{
		if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_INFO) && mem_comp(Packet.m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
		{
			// we got ze info
			CUnpacker Unpacker;
			Unpacker.Reset((unsigned char *)Packet.m_pData + sizeof(SERVERBROWSE_INFO), Packet.m_DataSize - sizeof(SERVERBROWSE_INFO));
			int Token = str_toint(Unpacker.GetString());
			if(Token != CurToken)
				continue;

			const int64_t EndTime = time_get();
			log_info("twping", "%g ms", (double)(EndTime - StartTime) / time_freq() * 1000);
			return 0;
		}
	}

	log_info("twping", "timeout (%" PRId64 " ms)", std::chrono::duration_cast<std::chrono::milliseconds>(Timeout).count());
	return 1;
}
