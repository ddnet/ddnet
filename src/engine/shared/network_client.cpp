#include "network.h"

#include <base/log.h>
#include <base/system.h>
#include <net/net.h>

CNetClient::~CNetClient()
{
	Close();
	if(m_pStun)
	{
		delete m_pStun;
		m_pStun = nullptr;
	}
}

bool CNetClient::Open(NETADDR BindAddr)
{
	// TODO: use the actual bind address, not just the port
        char aBindAddr[NETADDR_MAXSTRSIZE];
        str_format(aBindAddr, sizeof(aBindAddr), "0.0.0.0:%d", BindAddr.port);

        if(false
                || ddnet_net_new(&m_pNet)
                || ddnet_net_set_bindaddr(m_pNet, aBindAddr, str_length(aBindAddr))
                || ddnet_net_open(m_pNet))
        {
                log_error("net", "couldn't open net client: %s", ddnet_net_error(m_pNet));
                ddnet_net_free(m_pNet);
                m_pNet = nullptr;
                return false;
        }

	NETADDR Any = {0};
	Any.type = NETTYPE_IPV4 | NETTYPE_IPV6;
	m_pStun = new CStun(net_udp_create(Any));
        return true;
}

int CNetClient::Close()
{
	if(m_pNet)
	{
		ddnet_net_free(m_pNet);
		m_pNet = nullptr;
	}
	return 0;
}

int CNetClient::Disconnect(const char *pReason)
{
	if(m_PeerID != -1)
	{
		if(!pReason)
		{
			pReason = "";
		}
		if(ddnet_net_close(m_pNet, m_PeerID, pReason, str_length(pReason)))
		{
			log_error("net", "couldn't disconnect: %s", ddnet_net_error(m_pNet));
			exit(1);
		}
		m_PeerID = -1;
		m_State = NETSTATE_OFFLINE;
	}
	return 0;
}

int CNetClient::Update()
{
	// TODO: call timeout stuff
	return 0;
}

void CNetClient::Wait(uint64_t Microseconds)
{
	if(ddnet_net_wait_timeout(m_pNet, Microseconds * 1000))
	{
		log_error("net", "wait failed: %s", ddnet_net_error(m_pNet));
		exit(1);
	}
}

int CNetClient::Connect(const NETADDR *pAddr, int NumAddrs)
{
	Disconnect(nullptr);

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&pAddr[0], aAddr, sizeof(aAddr), true);
	char aUrl[128];
	str_format(aUrl, sizeof(aUrl), "ddnet-15+quic://%s#0026a0d653cd5f38d1002bf166167933f2f7910f26d6dd619b2b3fe769e057eef", aAddr);
	uint64_t PeerID;
	if(ddnet_net_connect(m_pNet, aUrl, str_length(aUrl), &PeerID))
	{
		log_error("net", "couldn't connect: %s", ddnet_net_error(m_pNet));
		exit(1);
	}
	m_PeerID = PeerID;
	m_State = NETSTATE_CONNECTING;
	return 0;
}

int CNetClient::ResetErrorString()
{
	// unimplemented
	return 0;
}

int CNetClient::Recv(CNetChunk *pChunk)
{
	while(true)
	{
		uint64_t PeerID;
		DdnetNetEvent Event;
		// Keep space for null termination.
                if(ddnet_net_recv(m_pNet, m_aBuffer, sizeof(m_aBuffer) - 1, &PeerID, &Event))
		{
			log_error("net", "recv failed: %s", ddnet_net_error(m_pNet));
                        exit(1);
		}
		switch(ddnet_net_ev_kind(&Event))
                {
                case DDNET_NET_EV_NONE:
                        return 0;
                case DDNET_NET_EV_CONNECT:
			if((int)PeerID != m_PeerID)
			{
				continue;
			}
			m_State = NETSTATE_ONLINE;
                        break;
                case DDNET_NET_EV_DISCONNECT:
			if((int)PeerID != m_PeerID)
			{
				continue;
			}
			m_PeerID = -1;
			m_State = NETSTATE_OFFLINE;
                        break;
                case DDNET_NET_EV_CHUNK:
			if((int)PeerID != m_PeerID)
			{
				continue;
			}
                        mem_zero(pChunk, sizeof(*pChunk));
                        pChunk->m_ClientID = 0;
                        pChunk->m_Flags = 0;
                        if(!ddnet_net_ev_chunk_is_unreliable(&Event))
                        {
                                pChunk->m_Flags |= NET_CHUNKFLAG_VITAL;
                        }
                        pChunk->m_DataSize = ddnet_net_ev_chunk_len(&Event);
                        pChunk->m_pData = m_aBuffer;
                        return 1;
                }
	}
}

int CNetClient::Send(CNetChunk *pChunk)
{
	if(pChunk->m_DataSize >= NET_MAX_PAYLOAD)
	{
		dbg_msg("netclient", "chunk payload too big. %d. dropping chunk", pChunk->m_DataSize);
		return -1;
	}

	if(pChunk->m_Flags & NETSENDFLAG_CONNLESS)
	{
		// unimplemented
	}
	else
	{
		if(m_PeerID == -1)
		{
			return -1;
		}
		dbg_assert(pChunk->m_ClientID == 0, "erroneous client id");
		if(ddnet_net_send_chunk(m_pNet, m_PeerID, (unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0))
		{
			log_error("net", "send failed: %s", ddnet_net_error(m_pNet));
			exit(1);
		}
		if((pChunk->m_Flags & NETSENDFLAG_FLUSH) != 0)
		{
			if(ddnet_net_flush(m_pNet, m_PeerID))
			{
				log_error("net", "flush failed: %s", ddnet_net_error(m_pNet));
				exit(1);
			}
		}
	}
	return 0;
}

int CNetClient::State()
{
	return m_State;
}

int CNetClient::Flush()
{
	// unimplemented
	return 0;
}

int CNetClient::GotProblems(int64_t MaxLatency) const
{
	// unimplemented
//	if(time_get() - m_Connection.LastRecvTime() > MaxLatency)
//		return 1;
	return 0;
}

const char *CNetClient::ErrorString() const
{
	// unimplemented
	return "";
}

void CNetClient::FeedStunServer(NETADDR StunServer)
{
	m_pStun->FeedStunServer(StunServer);
}

void CNetClient::RefreshStun()
{
	m_pStun->Refresh();
}

CONNECTIVITY CNetClient::GetConnectivity(int NetType, NETADDR *pGlobalAddr)
{
	return m_pStun->GetConnectivity(NetType, pGlobalAddr);
}
int CNetClient::NetType() const
{
	// unimplemented
	return NETTYPE_IPV4 | NETTYPE_IPV6;
}
const NETADDR *CNetClient::ServerAddress() const
{
	static const NETADDR Null = {0};
	return &Null;
}
void CNetClient::ConnectAddresses(const NETADDR **ppAddrs, int *pNumAddrs) const
{
	static const NETADDR Null = {0};
	*ppAddrs = &Null;
	*pNumAddrs = 1;
}
