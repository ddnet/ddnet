/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/logger.h>
#include <base/system.h>

#include <cstdlib>
#include <iterator> // std::size

#include <thread>

struct SPacket
{
	SPacket *m_pPrev;
	SPacket *m_pNext;

	NETADDR m_SendTo;
	int64_t m_Timestamp;
	int m_Id;
	int m_DataSize;
	char m_aData[1];
};

static SPacket *g_pFirst = (SPacket *)nullptr;
static SPacket *g_pLast = (SPacket *)nullptr;
static int g_CurrentLatency = 0;

struct SPingConfig
{
	int m_Base;
	int m_Flux;
	int m_Spike;
	int m_Loss;
	int m_Delay;
	int m_DelayFreq;
};

static SPingConfig g_aConfigPings[] = {
	// base flux spike loss delay delayfreq
	{0, 0, 0, 0, 0, 0},
	{40, 20, 100, 0, 0, 0},
	{140, 40, 200, 0, 0, 0},
};

static int g_ConfigNumpingconfs = std::size(g_aConfigPings);
static int g_ConfigInterval = 10; // seconds between different pingconfigs
static int g_ConfigLog = 0;
static int g_ConfigReorder = 0;

void Run(unsigned short Port, NETADDR Dest)
{
	NETADDR Src = {NETTYPE_IPV4, {0, 0, 0, 0}, Port};
	NETSOCKET Socket = net_udp_create(Src);

	int Id = 0;
	int Delaycounter = 0;

	while(true)
	{
		static int s_Lastcfg = 0;
		int n = ((time_get() / time_freq()) / g_ConfigInterval) % g_ConfigNumpingconfs;
		SPingConfig Ping = g_aConfigPings[n];

		if(n != s_Lastcfg)
			dbg_msg("crapnet", "cfg = %d", n);
		s_Lastcfg = n;

		// handle incoming packets
		while(true)
		{
			// fetch data
			int DataTrash = 0;
			NETADDR From;
			unsigned char *pData;
			int Bytes = net_udp_recv(Socket, &From, &pData);
			if(Bytes <= 0)
				break;

			if((rand() % 100) < Ping.m_Loss) // drop the packet
			{
				if(g_ConfigLog)
					dbg_msg("crapnet", "dropped packet");
				continue;
			}

			// create new packet
			SPacket *p = (SPacket *)malloc(sizeof(SPacket) + Bytes);

			if(net_addr_comp(&From, &Dest) == 0)
				p->m_SendTo = Src; // from the server
			else
			{
				Src = From; // from the client
				p->m_SendTo = Dest;
			}

			// queue packet
			p->m_pPrev = g_pLast;
			p->m_pNext = nullptr;
			if(g_pLast)
				g_pLast->m_pNext = p;
			else
			{
				g_pFirst = p;
				g_pLast = p;
			}
			g_pLast = p;

			// set data in packet
			p->m_Timestamp = time_get();
			p->m_DataSize = Bytes;
			p->m_Id = Id++;
			mem_copy(p->m_aData, pData, Bytes);

			if(Id > 20 && Bytes > 6 && DataTrash)
			{
				p->m_aData[6 + (rand() % (Bytes - 6))] = rand() & 255; // modify a byte
				if((rand() % 10) == 0)
				{
					p->m_DataSize -= rand() % 32;
					if(p->m_DataSize < 6)
						p->m_DataSize = 6;
				}
			}

			if(Delaycounter <= 0)
			{
				if(Ping.m_Delay)
					p->m_Timestamp += (time_freq() * 1000) / Ping.m_Delay;
				Delaycounter = Ping.m_DelayFreq;
			}
			Delaycounter--;

			if(g_ConfigLog)
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(&From, aAddrStr, sizeof(aAddrStr), true);
				dbg_msg("crapnet", "<< %08d %s (%d)", p->m_Id, aAddrStr, p->m_DataSize);
			}
		}

		SPacket *pNext = g_pFirst;
		while(true)
		{
			SPacket *p = pNext;
			if(!p)
				break;
			pNext = p->m_pNext;

			if((time_get() - p->m_Timestamp) > g_CurrentLatency)
			{
				char aFlags[] = "  ";

				if(g_ConfigReorder && (rand() % 2) == 0 && p->m_pNext)
				{
					aFlags[0] = 'R';
					p = g_pFirst->m_pNext;
				}

				if(p->m_pNext)
					p->m_pNext->m_pPrev = p->m_pPrev;
				else
					g_pLast = p->m_pPrev;

				if(p->m_pPrev)
					p->m_pPrev->m_pNext = p->m_pNext;
				else
					g_pFirst = p->m_pNext;

				// send and remove packet
				net_udp_send(Socket, &p->m_SendTo, p->m_aData, p->m_DataSize);

				// update lag
				double Flux = rand() / (double)RAND_MAX;
				int MsSpike = Ping.m_Spike;
				int MsFlux = Ping.m_Flux;
				int MsPing = Ping.m_Base;
				g_CurrentLatency = ((time_freq() * MsPing) / 1000) + (int64_t)(((time_freq() * MsFlux) / 1000) * Flux); // 50ms

				if(MsSpike && (p->m_Id % 100) == 0)
				{
					g_CurrentLatency += (time_freq() * MsSpike) / 1000;
					aFlags[1] = 'S';
				}

				if(g_ConfigLog)
				{
					char aAddrStr[NETADDR_MAXSTRSIZE];
					net_addr_str(&p->m_SendTo, aAddrStr, sizeof(aAddrStr), true);
					dbg_msg("crapnet", ">> %08d %s (%d) %s", p->m_Id, aAddrStr, p->m_DataSize, aFlags);
				}

				free(p);
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();
	NETADDR Addr = {NETTYPE_IPV4, {127, 0, 0, 1}, 8303};
	Run(8302, Addr);
	return 0;
}
