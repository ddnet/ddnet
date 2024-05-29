#include "network.h"

#include <base/log.h>
#include <base/system.h>
#include <net/net.h>

#include <curl/curl.h>

#define EE(function, net, ...) \
	do \
	{ \
		if(function(net, __VA_ARGS__)) \
		{ \
			ExitWithError(net, #function); \
		} \
	} while(0)

static void ExitWithError(CNet *pNet, const char *pFunction)
{
	log_error("net", "%s: %s", pFunction, ddnet_net_error(pNet));
	exit(1);
}

static bool AddrFromUrl(const char *pUrl, NETADDR *pAddr)
{
	// TODO: maybe parse URL by ourselves
	CURLU *pHandle = curl_url();
	char *pHostname;
	char *pPort;
	bool Error = false
		|| curl_url_set(pHandle, CURLUPART_URL, pUrl, CURLU_NON_SUPPORT_SCHEME)
		|| curl_url_get(pHandle, CURLUPART_HOST, &pHostname, 0)
		|| curl_url_get(pHandle, CURLUPART_PORT, &pPort, 0);
	curl_url_cleanup(pHandle);
	if(Error)
	{
		return false;
	}
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s:%s", pHostname, pPort);
	if(net_addr_from_str(pAddr, aBuf))
	{
		return false;
	}
	return true;
}

static bool Tw06AddrFromUrl(const char *pUrl, NETADDR *pAddr)
{
	// TODO: maybe parse URL by ourselves
	CURLU *pHandle = curl_url();
	char *pScheme;
	char *pHostname;
	char *pPort;
	bool Error = false
		|| curl_url_set(pHandle, CURLUPART_URL, pUrl, CURLU_NON_SUPPORT_SCHEME)
		|| curl_url_get(pHandle, CURLUPART_SCHEME, &pScheme, 0)
		|| curl_url_get(pHandle, CURLUPART_HOST, &pHostname, 0)
		|| curl_url_get(pHandle, CURLUPART_PORT, &pPort, 0);
	curl_url_cleanup(pHandle);
	if(Error)
	{
		return false;
	}
	if(str_comp(pScheme, "tw-0.6+udp") != 0)
	{
		return false;
	}
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s:%s", pHostname, pPort);
	if(net_addr_from_str(pAddr, aBuf))
	{
		return false;
	}
	return true;
}

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

	ddnet_net_ev_new(&m_pNetEvent);
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

	// TODO: use the same socket as the above one
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
	if(m_pNetEvent)
	{
		ddnet_net_ev_free(m_pNetEvent);
		m_pNetEvent = nullptr;
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
		EE(ddnet_net_close, m_pNet, m_PeerID, pReason, str_length(pReason));
		str_copy((char *)m_aBuffer, pReason, sizeof(m_aBuffer));
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
	EE(ddnet_net_wait_timeout, m_pNet, Microseconds * 1000);
}

int CNetClient::Connect(const NETADDR *pAddr, int NumAddrs)
{
	Disconnect(nullptr);

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&pAddr[0], aAddr, sizeof(aAddr), true);
	char aUrl[128];
	//str_format(aUrl, sizeof(aUrl), "ddnet-15+quic://%s#0026a0d653cd5f38d1002bf166167933f2f7910f26d6dd619b2b3fe769e057ee", aAddr);
	str_format(aUrl, sizeof(aUrl), "tw-0.6+udp://%s", aAddr);
	uint64_t PeerID;
	EE(ddnet_net_connect, m_pNet, aUrl, str_length(aUrl), &PeerID);
	m_PeerID = PeerID;
	m_State = NETSTATE_CONNECTING;
	return 0;
}

int CNetClient::ResetErrorString()
{
	dbg_assert(m_State == NETSTATE_OFFLINE, "can only reset error string while having one");
	if(m_State == NETSTATE_OFFLINE)
	{
		m_aBuffer[0] = 0;
	}
	return 0;
}

int CNetClient::Recv(CNetChunk *pChunk)
{
	while(true)
	{
		// Keep space for null termination.
                EE(ddnet_net_recv, m_pNet, m_aBuffer, sizeof(m_aBuffer) - 1, m_pNetEvent);
		switch(ddnet_net_ev_kind(m_pNetEvent))
                {
                case DDNET_NET_EV_NONE:
                        return 0;
                case DDNET_NET_EV_CONNECT:
			{
				uint64_t PeerID = ddnet_net_ev_connect_peer_index(m_pNetEvent);
				if((int)PeerID != m_PeerID)
				{
					continue;
				}
				const char *pAddr;
				size_t AddrLen;
				ddnet_net_ev_connect_addr(m_pNetEvent, &pAddr, &AddrLen);
				NETADDR Addr;
				if(!AddrFromUrl(pAddr, &Addr))
				{
					static const char UNRECOGNIZED_ADDR[] = "Unrecognized address";
					EE(ddnet_net_close, m_pNet, PeerID, UNRECOGNIZED_ADDR, sizeof(UNRECOGNIZED_ADDR) - 1);
					continue;
				}
				m_ServerAddress = Addr;
				m_State = NETSTATE_ONLINE;
			}
                        break;
                case DDNET_NET_EV_DISCONNECT:
			{
				uint64_t PeerID = ddnet_net_ev_disconnect_peer_index(m_pNetEvent);
				if((int)PeerID != m_PeerID)
				{
					continue;
				}
				m_PeerID = -1;
				m_State = NETSTATE_OFFLINE;
				log_debug("net", "reason len: %d", (int)ddnet_net_ev_disconnect_reason_len(m_pNetEvent));
				m_aBuffer[ddnet_net_ev_disconnect_reason_len(m_pNetEvent)] = 0;
			}
                        break;
                case DDNET_NET_EV_CHUNK:
			{
				uint64_t PeerID = ddnet_net_ev_chunk_peer_index(m_pNetEvent);
				if((int)PeerID != m_PeerID)
				{
					continue;
				}
				mem_zero(pChunk, sizeof(*pChunk));
				pChunk->m_ClientID = 0;
				pChunk->m_Flags = 0;
				if(!ddnet_net_ev_chunk_is_unreliable(m_pNetEvent))
				{
					pChunk->m_Flags |= NET_CHUNKFLAG_VITAL;
				}
				pChunk->m_DataSize = ddnet_net_ev_chunk_len(m_pNetEvent);
				pChunk->m_pData = m_aBuffer;
			}
                        return 1;
		case DDNET_NET_EV_CONNLESS_CHUNK:
			{
				const char *pAddr;
				size_t AddrLen;
				ddnet_net_ev_connless_chunk_addr(m_pNetEvent, &pAddr, &AddrLen);
				NETADDR Addr;
				if(!Tw06AddrFromUrl(pAddr, &Addr))
				{
					continue;
				}
				mem_zero(pChunk, sizeof(*pChunk));
				pChunk->m_ClientID = -1;
				pChunk->m_Address = Addr;
				pChunk->m_Flags = NETSENDFLAG_CONNLESS;
				pChunk->m_DataSize = ddnet_net_ev_connless_chunk_len(m_pNetEvent);
				pChunk->m_pData = m_aBuffer;
			}
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
		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&pChunk->m_Address, aAddr, sizeof(aAddr), true);
		char aUrl[128];
		str_format(aUrl, sizeof(aUrl), "tw-0.6+udp://%s", aAddr);
		EE(ddnet_net_send_connless_chunk, m_pNet, aUrl, str_length(aUrl), (const unsigned char *)pChunk->m_pData, pChunk->m_DataSize);
	}
	else
	{
		// TODO: remove?
		if(m_PeerID == -1)
		{
			return -1;
		}
		dbg_assert(pChunk->m_ClientID == 0, "erroneous client id");
		EE(ddnet_net_send_chunk, m_pNet, m_PeerID, (unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0);
		if((pChunk->m_Flags & NETSENDFLAG_FLUSH) != 0)
		{
			EE(ddnet_net_flush, m_pNet, m_PeerID);
		}
	}
	return 0;
}

int CNetClient::State()
{
	return m_State;
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
	if(m_State == NETSTATE_OFFLINE)
	{
		return (char *)m_aBuffer;
	}
	else
	{
		return "";
	}
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
	if(m_State != NETSTATE_ONLINE)
	{
		static const NETADDR Null = {0};
		return &Null;
	}
	return &m_ServerAddress;
}
void CNetClient::ConnectAddresses(const NETADDR **ppAddrs, int *pNumAddrs) const
{
	static const NETADDR Null = {0};
	*ppAddrs = &Null;
	*pNumAddrs = 1;
}
