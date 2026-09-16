/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "network.h"

#ifdef CONF_NETWORKING_QUIC

#include <base/dbg.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/str.h>

#include <curl/curl.h>
#include <net/net.h>

#include <algorithm>
#include <cstdlib>

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
	bool Error = false ||
		     curl_url_set(pHandle, CURLUPART_URL, pUrl, CURLU_NON_SUPPORT_SCHEME) ||
		     curl_url_get(pHandle, CURLUPART_HOST, &pHostname, 0) ||
		     curl_url_get(pHandle, CURLUPART_PORT, &pPort, 0);
	curl_url_cleanup(pHandle);
	if(Error)
	{
		return false;
	}
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s:%s", pHostname, pPort);
	return net_addr_from_str(pAddr, aBuf) == 0;
}

static bool Tw06AddrFromUrl(const char *pUrl, NETADDR *pAddr)
{
	// TODO: maybe parse URL by ourselves
	CURLU *pHandle = curl_url();
	char *pScheme;
	char *pHostname;
	char *pPort;
	bool Error = false ||
		     curl_url_set(pHandle, CURLUPART_URL, pUrl, CURLU_NON_SUPPORT_SCHEME) ||
		     curl_url_get(pHandle, CURLUPART_SCHEME, &pScheme, 0) ||
		     curl_url_get(pHandle, CURLUPART_HOST, &pHostname, 0) ||
		     curl_url_get(pHandle, CURLUPART_PORT, &pPort, 0);
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
	return net_addr_from_str(pAddr, aBuf) == 0;
}

CNetClient::~CNetClient()
{
	Close();
}

bool CNetClient::Open(NETADDR BindAddr)
{
	Close();

	// TODO: use the actual bind address, not just the port
	char aBindAddr[NETADDR_MAXSTRSIZE];
	str_format(aBindAddr, sizeof(aBindAddr), "0.0.0.0:%d", BindAddr.port);

	ddnet_net_ev_new(&m_pNetEvent);
	if(false ||
		ddnet_net_new(&m_pNet) ||
		ddnet_net_set_bindaddr(m_pNet, aBindAddr, str_length(aBindAddr)) ||
		ddnet_net_open(m_pNet))
	{
		log_error("net", "couldn't open net client: %s", ddnet_net_error(m_pNet));
		ddnet_net_free(m_pNet);
		m_pNet = nullptr;
		ddnet_net_ev_free(m_pNetEvent);
		m_pNetEvent = nullptr;
		return false;
	}

	// TODO: use the same socket as the one of the network library
	NETADDR Any = {0};
	Any.type = NETTYPE_IPV4 | NETTYPE_IPV6;
	m_pStun = new CStun(net_udp_create(Any));

	m_State = NETSTATE_OFFLINE;
	m_PeerId = -1;
	m_NumConnectAddrs = 0;
	m_aErrorString[0] = '\0';
	return true;
}

void CNetClient::Close()
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
	if(m_pStun)
	{
		delete m_pStun;
		m_pStun = nullptr;
	}
	m_State = NETSTATE_OFFLINE;
	m_PeerId = -1;
}

void CNetClient::Disconnect(const char *pReason)
{
	if(m_PeerId != -1)
	{
		if(!pReason)
		{
			pReason = "";
		}
		EE(ddnet_net_close, m_pNet, m_PeerId, pReason, str_length(pReason));
		str_copy(m_aErrorString, pReason);
		m_PeerId = -1;
		m_State = NETSTATE_OFFLINE;
	}
}

void CNetClient::Connect(const NETADDR *pAddr, int NumAddrs)
{
	Disconnect(nullptr);

	m_NumConnectAddrs = std::min(NumAddrs, (int)std::size(m_aConnectAddrs));
	for(int i = 0; i < m_NumConnectAddrs; i++)
	{
		m_aConnectAddrs[i] = pAddr[i];
	}

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&pAddr[0], aAddr, sizeof(aAddr), true);
	char aUrl[128];
	// TODO: connect via `ddnet-18+quic://` when the server advertises support for it
	str_format(aUrl, sizeof(aUrl), "tw-0.6+udp://%s", aAddr);
	uint64_t PeerId;
	EE(ddnet_net_connect, m_pNet, aUrl, str_length(aUrl), &PeerId);
	m_PeerId = PeerId;
	m_State = NETSTATE_CONNECTING;
	m_aErrorString[0] = '\0';
}

void CNetClient::Connect7(const NETADDR *pAddr, int NumAddrs)
{
	// TODO: 0.7 is not supported by the network library yet.
	Disconnect(nullptr);
	str_copy(m_aErrorString, "0.7 servers are not supported by this client");
}

void CNetClient::Update()
{
	// TODO: call timeout stuff
	if(m_pStun)
	{
		m_pStun->Update();
	}
}

void CNetClient::Wait(uint64_t Microseconds)
{
	EE(ddnet_net_wait_timeout, m_pNet, Microseconds * 1000);
}

int CNetClient::Flush()
{
	if(m_PeerId == -1)
	{
		return 0;
	}
	EE(ddnet_net_flush, m_pNet, m_PeerId);
	return 0;
}

void CNetClient::ResetErrorString()
{
	dbg_assert(m_State == NETSTATE_OFFLINE, "can only reset error string while having one");
	m_aErrorString[0] = '\0';
}

int CNetClient::Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken, bool Sixup)
{
	*pResponseToken = NET_SECURITY_TOKEN_UNKNOWN;

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
			const uint64_t PeerId = ddnet_net_ev_connect_peer_index(m_pNetEvent);
			if((int64_t)PeerId != m_PeerId)
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
				EE(ddnet_net_close, m_pNet, PeerId, UNRECOGNIZED_ADDR, sizeof(UNRECOGNIZED_ADDR) - 1);
				continue;
			}
			m_ServerAddress = Addr;
			m_State = NETSTATE_ONLINE;
		}
		break;
		case DDNET_NET_EV_DISCONNECT:
		{
			const uint64_t PeerId = ddnet_net_ev_disconnect_peer_index(m_pNetEvent);
			if((int64_t)PeerId != m_PeerId)
			{
				continue;
			}
			m_PeerId = -1;
			m_State = NETSTATE_OFFLINE;
			const size_t ReasonLen = std::min(ddnet_net_ev_disconnect_reason_len(m_pNetEvent), sizeof(m_aErrorString) - 1);
			mem_copy(m_aErrorString, m_aBuffer, ReasonLen);
			m_aErrorString[ReasonLen] = '\0';
		}
		break;
		case DDNET_NET_EV_CHUNK:
		{
			const uint64_t PeerId = ddnet_net_ev_chunk_peer_index(m_pNetEvent);
			if((int64_t)PeerId != m_PeerId)
			{
				continue;
			}
			mem_zero(pChunk, sizeof(*pChunk));
			pChunk->m_ClientId = 0;
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
			pChunk->m_ClientId = -1;
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
	pChunk->AssertSizeSanity();

	if(pChunk->m_Flags & NETSENDFLAG_CONNLESS)
	{
		// TODO: the extended connless header is not supported by the network library
		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&pChunk->m_Address, aAddr, sizeof(aAddr), true);
		char aUrl[128];
		str_format(aUrl, sizeof(aUrl), "tw-0.6+udp://%s", aAddr);
		EE(ddnet_net_send_connless_chunk, m_pNet, aUrl, str_length(aUrl), (const unsigned char *)pChunk->m_pData, pChunk->m_DataSize);
		return 0;
	}

	if(m_PeerId == -1)
	{
		return -1;
	}
	dbg_assert(pChunk->m_ClientId == 0, "erroneous client id");
	EE(ddnet_net_send_chunk, m_pNet, m_PeerId, (const unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0);
	if((pChunk->m_Flags & NETSENDFLAG_FLUSH) != 0)
	{
		EE(ddnet_net_flush, m_pNet, m_PeerId);
	}
	return 0;
}

int CNetClient::State()
{
	return m_State;
}

bool CNetClient::GotProblems(int64_t MaxLatency) const
{
	// TODO: the network library does not report the last receive time yet
	return false;
}

const char *CNetClient::ErrorString() const
{
	if(m_State == NETSTATE_OFFLINE)
	{
		return m_aErrorString;
	}
	return "";
}

void CNetClient::FeedStunServer(NETADDR StunServer)
{
	if(m_pStun)
	{
		m_pStun->FeedStunServer(StunServer);
	}
}

void CNetClient::RefreshStun()
{
	if(m_pStun)
	{
		m_pStun->Refresh();
	}
}

CONNECTIVITY CNetClient::GetConnectivity(int NetType, NETADDR *pGlobalAddr)
{
	if(!m_pStun)
	{
		return CONNECTIVITY::UNKNOWN;
	}
	return m_pStun->GetConnectivity(NetType, pGlobalAddr);
}

int CNetClient::NetType()
{
	// unimplemented
	return NETTYPE_IPV4 | NETTYPE_IPV6;
}

bool CNetClient::SocketIsBroken() const
{
	// the network library reports errors through ErrorString()
	return false;
}

const NETADDR *CNetClient::ServerAddress() const
{
	return &m_ServerAddress;
}

void CNetClient::ConnectAddresses(const NETADDR **ppAddrs, int *pNumAddrs) const
{
	*ppAddrs = m_aConnectAddrs;
	*pNumAddrs = m_NumConnectAddrs;
}

#endif // CONF_NETWORKING_QUIC
