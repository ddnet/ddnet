#include "network.h"

#include <base/log.h>
#include <net/net.h>

static unsigned char IDENTITY[] = {
        0x5c, 0xf2, 0xa4, 0xf0, 0xed, 0x3d, 0xc8, 0x5b, 0x3f, 0x4b, 0xfa, 0x5c,
        0xa9, 0x7b, 0x8a, 0xde, 0xaf, 0x0d, 0x5e, 0xc1, 0x65, 0x17, 0x9a, 0xf8,
        0x05, 0xa3, 0xf7, 0x75, 0x1d, 0xd8, 0xac, 0x47
};

CNetServer::~CNetServer()
{
	Close();
}

bool CNetServer::Open(NETADDR BindAddr, CNetBan *pNetBan, int MaxClients, int MaxClientsPerIP)
{
	// TODO: use the actual bind address, not just the port
	char aBindAddr[NETADDR_MAXSTRSIZE];
	str_format(aBindAddr, sizeof(aBindAddr), "0.0.0.0:%d", BindAddr.port);

	if(false
		|| ddnet_net_new(&m_pNet)
		|| ddnet_net_set_bindaddr(m_pNet, aBindAddr, str_length(aBindAddr))
		|| ddnet_net_set_identity(m_pNet, &IDENTITY)
		|| ddnet_net_set_accept_incoming_connections(m_pNet, true)
		|| ddnet_net_open(m_pNet))
	{
		log_error("net", "couldn't open net server: %s", ddnet_net_error(m_pNet));
		ddnet_net_free(m_pNet);
		m_pNet = nullptr;
		return false;
	}
	return true;
}

int CNetServer::SetCallbacks(NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser)
{
	m_pfnNewClient = pfnNewClient;
	m_pfnDelClient = pfnDelClient;
	m_pUser = pUser;
	return 0;
}

int CNetServer::SetCallbacks(NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_NEWCLIENT_NOAUTH pfnNewClientNoAuth, NETFUNC_CLIENTREJOIN pfnClientRejoin, NETFUNC_DELCLIENT pfnDelClient, void *pUser)
{
	return SetCallbacks(pfnNewClient, pfnDelClient, pUser);
}

int CNetServer::Close()
{
	if(m_pNet)
	{
		ddnet_net_free(m_pNet);
		m_pNet = nullptr;
	}
	return 0;
}

int CNetServer::Drop(int ClientID, const char *pReason)
{
	if(m_pfnDelClient)
	{
		m_pfnDelClient(ClientID, pReason, m_pUser);
	}

	if(ddnet_net_close(m_pNet, ClientID, pReason, str_length(pReason)))
	{
		log_error("net", "drop failed: %s", ddnet_net_error(m_pNet));
		exit(1);
	}
	return 0;
}

int CNetServer::Update()
{
	// TODO: call timeout stuff
	return 0;
}

void CNetServer::Wait(uint64_t Microseconds)
{
	if(ddnet_net_wait_timeout(m_pNet, Microseconds * 1000))
	{
		log_error("net", "wait failed: %s", ddnet_net_error(m_pNet));
		exit(1);
	}
}

int CNetServer::Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken)
{
	while(true)
	{
		uint64_t ClientID;
		DdnetNetEvent Event;
		// Keep space for null termination.
		if(ddnet_net_recv(m_pNet, m_aBuffer, sizeof(m_aBuffer) - 1, &ClientID, &Event))
		{
			log_error("net", "recv failed: %s", ddnet_net_error(m_pNet));
			exit(1);
		}
		switch(ddnet_net_ev_kind(&Event))
		{
		case DDNET_NET_EV_NONE:
			return 0;
		case DDNET_NET_EV_CONNECT:
			if(m_pfnNewClient)
			{
				m_pfnNewClient(ClientID, m_pUser, false);
			}
			break;
		case DDNET_NET_EV_DISCONNECT:
			if(m_pfnDelClient)
			{
				m_aBuffer[ddnet_net_ev_disconnect_reason_len(&Event)] = 0;
				const char *pReason = ddnet_net_ev_disconnect_is_remote(&Event) ? "" : (char *)m_aBuffer;
				m_pfnDelClient(ClientID, pReason, m_pUser);
			}
			break;
		case DDNET_NET_EV_CHUNK:
			mem_zero(pChunk, sizeof(*pChunk));
			pChunk->m_ClientID = ClientID;
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

int CNetServer::Send(CNetChunk *pChunk)
{
	if(pChunk->m_DataSize >= NET_MAX_PAYLOAD)
	{
		dbg_msg("netserver", "packet payload too big. %d. dropping packet", pChunk->m_DataSize);
		return -1;
	}

	if(pChunk->m_Flags & NETSENDFLAG_CONNLESS)
	{
		// unimplemented
		return 0;
	}
	else
	{
		dbg_assert(pChunk->m_ClientID >= 0, "erroneous client id");
		dbg_assert(pChunk->m_ClientID < MaxClients(), "erroneous client id");
		if(ddnet_net_send_chunk(m_pNet, pChunk->m_ClientID, (unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0))
		{
			log_error("net", "send failed: %s", ddnet_net_error(m_pNet));
			exit(1);
		}
		// TODO: handle flush
	}
	return 0;
}

int CNetServer::SendConnlessSixup(CNetChunk *pChunk, SECURITY_TOKEN ResponseToken)
{
	// unimplemented
	return 0;
}

void CNetServer::SetMaxClientsPerIP(int Max)
{
	// unimplemented
}

bool CNetServer::SetTimedOut(int ClientID, int OrigID)
{
	// unimplemented
	return false;
}

void CNetServer::SetTimeoutProtected(int ClientID)
{
	// unimplemented
}

int CNetServer::ResetErrorString(int ClientID)
{
	// unimplemented
	return 0;
}

const char *CNetServer::ErrorString(int ClientID)
{
	// unimplemented
	return "";
}

const NETADDR *CNetServer::ClientAddr(int ClientID) const
{
	// unimplemented
	static const NETADDR Null = {0};
	return &Null;
}

CNetBan *CNetServer::NetBan() const
{
	// unimplemented
	log_error("net", "net ban not implemented");
	exit(1);
}
NETADDR CNetServer::Address() const
{
	// unimplemented
	NETADDR Null = {0};
	return Null;
}
int CNetServer::MaxClients() const
{
	// unimplemented
	return 64;
}
bool CNetServer::HasSecurityToken(int ClientID) const
{
	return true;
}
SECURITY_TOKEN CNetServer::GetGlobalToken()
{
	// unimplemented
	return 0xdeadbeef;
}
