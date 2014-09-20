/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include <engine/shared/network.h>

#include <game/version.h>

#include "versionsrv.h"
#include "mapversions.h"

#include <zlib.h>

enum {
	MAX_MAPS_PER_PACKET=48,
	MAX_PACKETS=16,
	MAX_MAPS=MAX_MAPS_PER_PACKET*MAX_PACKETS,
};

struct CPacketData
{
	int m_Size;
	struct {
		unsigned char m_aHeader[sizeof(VERSIONSRV_MAPLIST)];
		CMapVersion m_aMaplist[MAX_MAPS_PER_PACKET];
	} m_Data;
};

CPacketData m_aPackets[MAX_PACKETS];
static int m_NumPackets = 0;
unsigned char m_aNews[NEWS_SIZE];

unsigned char m_aServerList[DDNETLIST_SIZE];
int m_ServerListPlainLength = 0;
int m_ServerListCompLength = 0;
bool m_ServerListLoaded = false;

static CNetClient g_NetOp; // main

void BuildPackets()
{
	CMapVersion *pCurrent = &s_aMapVersionList[0];
	int ServersLeft = s_NumMapVersionItems;
	m_NumPackets = 0;
	while(ServersLeft && m_NumPackets < MAX_PACKETS)
	{
		int Chunk = ServersLeft;
		if(Chunk > MAX_MAPS_PER_PACKET)
			Chunk = MAX_MAPS_PER_PACKET;
		ServersLeft -= Chunk;

		// copy header
		mem_copy(m_aPackets[m_NumPackets].m_Data.m_aHeader, VERSIONSRV_MAPLIST, sizeof(VERSIONSRV_MAPLIST));

		// copy map versions
		for(int i = 0; i < Chunk; i++)
		{
			m_aPackets[m_NumPackets].m_Data.m_aMaplist[i] = *pCurrent;
			pCurrent++;
		}

		m_aPackets[m_NumPackets].m_Size = sizeof(VERSIONSRV_MAPLIST) + sizeof(CMapVersion)*Chunk;

		m_NumPackets++;
	}
}

void ReadNews()
{
	IOHANDLE newsFile = io_open("news", IOFLAG_READ);
	if (!newsFile)
		return;

	io_read(newsFile, m_aNews, NEWS_SIZE);

	io_close(newsFile);
}

void ReadServerList()
{
	mem_zero(m_aServerList, sizeof(m_aServerList));

	IOHANDLE File = io_open("serverlist.json", IOFLAG_READ);
	if (!File)
		return;

	int PlainLength = io_length(File);
	char aPlain[16384];

	io_read(File, aPlain, sizeof(aPlain));
	io_close(File);

	// compress
	uLongf DstLen = DDNETLIST_SIZE;

	if (compress((Bytef*)m_aServerList, &DstLen, (Bytef*)aPlain, PlainLength) == Z_OK)
	{
		m_ServerListLoaded = true;
		m_ServerListPlainLength = PlainLength;
		m_ServerListCompLength = DstLen;
	}
}

void SendVer(NETADDR *pAddr)
{
	CNetChunk p;
	unsigned char aData[sizeof(VERSIONSRV_VERSION) + sizeof(GAME_RELEASE_VERSION)];

	mem_copy(aData, VERSIONSRV_VERSION, sizeof(VERSIONSRV_VERSION));
	mem_copy(aData + sizeof(VERSIONSRV_VERSION), GAME_RELEASE_VERSION, sizeof(GAME_RELEASE_VERSION));

	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_pData = aData;
	p.m_DataSize = sizeof(aData);

	g_NetOp.Send(&p);
}

void SendNews(NETADDR *pAddr)
{
	CNetChunk p;
	unsigned char aData[NEWS_SIZE + sizeof(VERSIONSRV_NEWS)];

	mem_copy(aData, VERSIONSRV_NEWS, sizeof(VERSIONSRV_NEWS));
	mem_copy(aData + sizeof(VERSIONSRV_NEWS), m_aNews, NEWS_SIZE);

	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_pData = aData;
	p.m_DataSize = sizeof(aData);

	g_NetOp.Send(&p);
}

// Packet: VERSIONSRV_DDNETLIST + char[4] Token + int16 comp_length + int16 plain_length + char[comp_length] 
void SendServerList(NETADDR *pAddr, const char *Token)
{
	CNetChunk p;
	unsigned char aData[16384];
	short WordCompLength = m_ServerListCompLength;
	short WordPlainLength = m_ServerListPlainLength;

	mem_copy(aData, VERSIONSRV_DDNETLIST, sizeof(VERSIONSRV_DDNETLIST));
	mem_copy(aData + sizeof(VERSIONSRV_DDNETLIST), Token, 4); // send back token
	mem_copy(aData + sizeof(VERSIONSRV_DDNETLIST)+4, &WordCompLength, 2); // compressed length
	mem_copy(aData + sizeof(VERSIONSRV_DDNETLIST)+6, &WordPlainLength, 2); // plain length
	mem_copy(aData + sizeof(VERSIONSRV_DDNETLIST)+8, m_aServerList, m_ServerListCompLength);

	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_pData = aData;
	p.m_DataSize = sizeof(VERSIONSRV_DDNETLIST) + 4 + 2 + 2 + m_ServerListCompLength;

	g_NetOp.Send(&p);
}

int main(int argc, char **argv) // ignore_convention
{
	NETADDR BindAddr;

	dbg_logger_stdout();
	net_init();

	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_ALL;
	BindAddr.port = VERSIONSRV_PORT;
	if(!g_NetOp.Open(BindAddr, 0))
	{
		dbg_msg("mastersrv", "couldn't start network");
		return -1;
	}

	BuildPackets();

	ReadNews();
	ReadServerList();

	dbg_msg("versionsrv", "started");

	while(1)
	{
		g_NetOp.Update();

		// process packets
		CNetChunk Packet;
		while(g_NetOp.Recv(&Packet))
		{
			if(Packet.m_DataSize == sizeof(VERSIONSRV_GETVERSION) &&
				mem_comp(Packet.m_pData, VERSIONSRV_GETVERSION, sizeof(VERSIONSRV_GETVERSION)) == 0)
			{
				SendVer(&Packet.m_Address);

				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(&Packet.m_Address, aAddrStr, sizeof(aAddrStr), false);
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "version request by %s", aAddrStr);
				dbg_msg("versionsrv", aBuf);
			}

			if(Packet.m_DataSize == sizeof(VERSIONSRV_GETNEWS) &&
				mem_comp(Packet.m_pData, VERSIONSRV_GETNEWS, sizeof(VERSIONSRV_GETNEWS)) == 0)
			{
				SendNews(&Packet.m_Address);

				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(&Packet.m_Address, aAddrStr, sizeof(aAddrStr), false);
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "news request by %s", aAddrStr);
				dbg_msg("versionsrv", aBuf);
			}

			if(Packet.m_DataSize == sizeof(VERSIONSRV_GETMAPLIST) &&
				mem_comp(Packet.m_pData, VERSIONSRV_GETMAPLIST, sizeof(VERSIONSRV_GETMAPLIST)) == 0)
			{
				CNetChunk p;
				p.m_ClientID = -1;
				p.m_Address = Packet.m_Address;
				p.m_Flags = NETSENDFLAG_CONNLESS;

				for(int i = 0; i < m_NumPackets; i++)
				{
					p.m_DataSize = m_aPackets[i].m_Size;
					p.m_pData = &m_aPackets[i].m_Data;
					g_NetOp.Send(&p);
				}
			}

			if(m_ServerListLoaded &&
				Packet.m_DataSize == sizeof(VERSIONSRV_GETDDNETLIST) + 4 &&
				mem_comp(Packet.m_pData, VERSIONSRV_GETDDNETLIST, sizeof(VERSIONSRV_GETDDNETLIST)) == 0)
			{
				char aToken[4];
				mem_copy(aToken, (char*)Packet.m_pData+sizeof(VERSIONSRV_GETDDNETLIST), 4);

				SendServerList(&Packet.m_Address, aToken);
			}
		}

		// wait for input
		net_socket_read_wait(g_NetOp.m_Socket, 1000);
	}

	return 0;
}
