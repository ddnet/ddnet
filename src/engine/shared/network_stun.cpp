#include "network.h"

#include <base/log.h>
#include <base/system.h>

static int IndexFromNetType(int NetType)
{
	switch(NetType)
	{
	case NETTYPE_IPV6:
		return 0;
	case NETTYPE_IPV4:
		return 1;
	}
	return -1;
}

static const char *IndexToSystem(int Index)
{
	switch(Index)
	{
	case 0:
		return "stun/6";
	case 1:
		return "stun/4";
	}
	dbg_break();
	return nullptr;
}

static int RetryWaitSeconds(int NumUnsuccessfulTries)
{
	return (1 << clamp(NumUnsuccessfulTries, 0, 9));
}

CStun::CProtocol::CProtocol(int Index, NETSOCKET Socket) :
	m_Index(Index),
	m_Socket(Socket)
{
	mem_zero(&m_StunServer, sizeof(NETADDR));
	// Initialize `m_Stun` with random data.
	unsigned char aBuf[32];
	StunMessagePrepare(aBuf, sizeof(aBuf), &m_Stun);
}

void CStun::CProtocol::FeedStunServer(NETADDR StunServer)
{
	if(m_HaveStunServer && net_addr_comp(&m_StunServer, &StunServer) == 0)
	{
		return;
	}
	m_HaveStunServer = true;
	m_StunServer = StunServer;
	m_NumUnsuccessfulTries = 0;
	Refresh();
}

void CStun::CProtocol::Refresh()
{
	m_NextTry = time_get();
}

void CStun::CProtocol::Update()
{
	int64_t Now = time_get();
	if(m_NextTry == -1 || Now < m_NextTry || !m_HaveStunServer)
	{
		return;
	}
	m_NextTry = Now + RetryWaitSeconds(m_NumUnsuccessfulTries) * time_freq();
	m_NumUnsuccessfulTries += 1;
	unsigned char aBuf[32];
	int Size = StunMessagePrepare(aBuf, sizeof(aBuf), &m_Stun);
	if(net_udp_send(m_Socket, &m_StunServer, aBuf, Size) == -1)
	{
		log_debug(IndexToSystem(m_Index), "couldn't send stun request");
		return;
	}
}

bool CStun::CProtocol::OnPacket(NETADDR Addr, unsigned char *pData, int DataSize)
{
	if(m_NextTry < 0 || !m_HaveStunServer)
	{
		return false;
	}
	bool Success;
	NETADDR StunAddr;
	if(StunMessageParse(pData, DataSize, &m_Stun, &Success, &StunAddr))
	{
		return false;
	}
	m_LastResponse = time_get();
	if(!Success)
	{
		m_HaveAddr = false;
		log_debug(IndexToSystem(m_Index), "got error response");
		return true;
	}
	m_NextTry = -1;
	m_NumUnsuccessfulTries = -1;
	m_HaveAddr = true;
	m_Addr = StunAddr;

	char aStunAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&StunAddr, aStunAddr, sizeof(aStunAddr), true);
	log_debug(IndexToSystem(m_Index), "got address: %s", aStunAddr);
	return true;
}

CONNECTIVITY CStun::CProtocol::GetConnectivity(NETADDR *pGlobalAddr)
{
	if(!m_HaveStunServer)
	{
		return CONNECTIVITY::UNKNOWN;
	}
	int64_t Now = time_get();
	int64_t Freq = time_freq();
	bool HaveTriedALittle = m_NumUnsuccessfulTries >= 5 && (m_LastResponse == -1 || Now - m_LastResponse >= 30 * Freq);
	if(m_LastResponse == -1 && !HaveTriedALittle)
	{
		return CONNECTIVITY::CHECKING;
	}
	else if(HaveTriedALittle)
	{
		return CONNECTIVITY::UNREACHABLE;
	}
	else if(!m_HaveAddr)
	{
		return CONNECTIVITY::REACHABLE;
	}
	else
	{
		*pGlobalAddr = m_Addr;
		return CONNECTIVITY::ADDRESS_KNOWN;
	}
}

CStun::CStun(NETSOCKET Socket) :
	m_aProtocols{CProtocol(0, Socket), CProtocol(1, Socket)}
{
}

void CStun::FeedStunServer(NETADDR StunServer)
{
	int Index = IndexFromNetType(StunServer.type);
	if(Index < 0)
	{
		return;
	}
	m_aProtocols[Index].FeedStunServer(StunServer);
}

void CStun::Refresh()
{
	for(auto &Protocol : m_aProtocols)
	{
		Protocol.Refresh();
	}
}

void CStun::Update()
{
	for(auto &Protocol : m_aProtocols)
	{
		Protocol.Update();
	}
}

bool CStun::OnPacket(NETADDR Addr, unsigned char *pData, int DataSize)
{
	int Index = IndexFromNetType(Addr.type);
	if(Index < 0)
	{
		return false;
	}
	return m_aProtocols[Index].OnPacket(Addr, pData, DataSize);
}

CONNECTIVITY CStun::GetConnectivity(int NetType, NETADDR *pGlobalAddr)
{
	int Index = IndexFromNetType(NetType);
	dbg_assert(Index != -1, "invalid nettype");
	return m_aProtocols[Index].GetConnectivity(pGlobalAddr);
}
