/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "network.h"

#ifdef CONF_NETWORKING_QUIC

#include "config.h"
#include "netban.h"

#include <base/dbg.h>
#include <base/hash_ctxt.h>
#include <base/log.h>
#include <base/math.h>
#include <base/net.h>
#include <base/secure.h>
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

void CNetServer::CPeer::Reset()
{
	m_State = STATE_NONE;
	m_Id = -1;
	m_TimeoutProtected = false;
	mem_zero(&m_Address, sizeof(m_Address));
	m_aAddressStr[0] = '\0';
	m_aAddressStrNoPort[0] = '\0';
}

void CNetServer::CPeer::SetAddress(const NETADDR &Addr)
{
	m_Address = Addr;
	net_addr_str(&m_Address, m_aAddressStr.data(), m_aAddressStr.size(), true);
	net_addr_str(&m_Address, m_aAddressStrNoPort.data(), m_aAddressStrNoPort.size(), false);
}

CNetServer::CNetServer()
{
	secure_random_fill(m_aIdentity, sizeof(m_aIdentity));
}

CNetServer::~CNetServer()
{
	Close();
}

bool CNetServer::Open(NETADDR BindAddr, CNetBan *pNetBan, int MaxClients, int MaxClientsPerIp)
{
	m_pNetBan = pNetBan;
	m_Address = BindAddr;
	m_MaxClients = std::clamp(MaxClients, 1, (int)NET_MAX_CLIENTS);
	m_MaxClientsPerIp = std::clamp(MaxClientsPerIp, 1, (int)NET_MAX_CLIENTS);

	secure_random_fill(m_aSecurityTokenSeed, sizeof(m_aSecurityTokenSeed));

	for(auto &Peer : m_aPeers)
	{
		Peer.Reset();
	}

	// TODO: use the actual bind address, not just the port
	char aBindAddr[NETADDR_MAXSTRSIZE];
	str_format(aBindAddr, sizeof(aBindAddr), "0.0.0.0:%d", BindAddr.port);

	ddnet_net_ev_new(&m_pNetEvent);
	if(false ||
		ddnet_net_new(&m_pNet) ||
		ddnet_net_set_bindaddr(m_pNet, aBindAddr, str_length(aBindAddr)) ||
		ddnet_net_set_identity(m_pNet, &m_aIdentity) ||
		ddnet_net_set_accept_connections(m_pNet, true) ||
		ddnet_net_open(m_pNet))
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
	m_pfnNewClientNoAuth = pfnNewClientNoAuth;
	m_pfnClientRejoin = pfnClientRejoin;
	return SetCallbacks(pfnNewClient, pfnDelClient, pUser);
}

void CNetServer::Close()
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
}

void CNetServer::Drop(int ClientId, const char *pReason)
{
	const uint64_t PeerId = m_aPeers[ClientId].m_Id;
	if(PeerId == (uint64_t)-1)
	{
		return;
	}

	if(m_pfnDelClient)
	{
		m_pfnDelClient(ClientId, pReason, m_pUser);
	}

	// Reset peer mapping.
	m_aPeers[ClientId].Reset();
	EE(ddnet_net_set_userdata, m_pNet, PeerId, (void *)(uintptr_t)-1);

	// Close the connection.
	EE(ddnet_net_close, m_pNet, PeerId, pReason, str_length(pReason));
}

void CNetServer::Update()
{
	// TODO: detect timeouts and honor timeout protection
}

void CNetServer::Wait(uint64_t Microseconds)
{
	EE(ddnet_net_wait_timeout, m_pNet, Microseconds * 1000);
}

void CNetServer::Flush(int ClientId)
{
	if(m_aPeers[ClientId].m_Id == (uint64_t)-1)
	{
		return;
	}
	EE(ddnet_net_flush, m_pNet, m_aPeers[ClientId].m_Id);
}

void CNetServer::EndFlushBatch()
{
	m_FlushBatch = false;
	for(int ClientId = 0; ClientId < MaxClients(); ClientId++)
	{
		if(!m_aFlushPending[ClientId])
			continue;
		m_aFlushPending[ClientId] = false;
		// The client may have been dropped while the batch was open.
		if(m_aPeers[ClientId].m_Id != (uint64_t)-1)
			Flush(ClientId);
	}
}

int CNetServer::Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken)
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

			char aBanReason[256];
			if(NetBan() && NetBan()->IsBanned(&Addr, aBanReason, sizeof(aBanReason)))
			{
				EE(ddnet_net_close, m_pNet, PeerId, aBanReason, str_length(aBanReason));
				continue;
			}

			uint32_t NumConnected;
			EE(ddnet_net_num_peers_in_bucket, m_pNet, pAddr, AddrLen, &NumConnected);
			if((int)NumConnected > m_MaxClientsPerIp)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Only %d players with the same IP are allowed", m_MaxClientsPerIp);
				EE(ddnet_net_close, m_pNet, PeerId, aBuf, str_length(aBuf));
				continue;
			}

			int ClientId = -1;
			for(int i = 0; i < MaxClients(); i++)
			{
				if(m_aPeers[m_NextClientId].m_Id == (uint64_t)-1)
				{
					ClientId = m_NextClientId;
					m_NextClientId = (m_NextClientId + 1) % MaxClients();
					break;
				}
				m_NextClientId = (m_NextClientId + 1) % MaxClients();
			}
			if(ClientId == -1)
			{
				static const char FULL[] = "This server is full";
				EE(ddnet_net_close, m_pNet, PeerId, FULL, sizeof(FULL) - 1);
				continue;
			}

			m_aPeers[ClientId].m_State = CPeer::STATE_CONNECTED;
			m_aPeers[ClientId].m_Id = PeerId;
			m_aPeers[ClientId].SetAddress(Addr);
			EE(ddnet_net_set_userdata, m_pNet, PeerId, (void *)(uintptr_t)ClientId);
			if(m_pfnNewClient)
			{
				m_pfnNewClient(ClientId, m_pUser, false);
			}
		}
		break;
		case DDNET_NET_EV_DISCONNECT:
		{
			const uint64_t PeerId = ddnet_net_ev_disconnect_peer_index(m_pNetEvent);
			void *pUserdata;
			EE(ddnet_net_userdata, m_pNet, PeerId, &pUserdata);
			if((uintptr_t)pUserdata == (uintptr_t)-1)
			{
				continue;
			}
			const int ClientId = (uintptr_t)pUserdata;
			dbg_assert(m_aPeers[ClientId].m_Id == PeerId, "invalid peer mapping");

			// The peer mapping has to be cleared before the callback, sends from
			// within the callback would otherwise go to a closed connection.
			m_aPeers[ClientId].m_Id = -1;
			m_aPeers[ClientId].m_State = CPeer::STATE_NONE;
			m_aFlushPending[ClientId] = false;

			if(m_pfnDelClient)
			{
				m_aBuffer[ddnet_net_ev_disconnect_reason_len(m_pNetEvent)] = 0;
				const char *pReason = ddnet_net_ev_disconnect_is_remote(m_pNetEvent) ? "" : (char *)m_aBuffer;
				m_pfnDelClient(ClientId, pReason, m_pUser);
			}
		}
		break;
		case DDNET_NET_EV_CHUNK:
		{
			const uint64_t PeerId = ddnet_net_ev_chunk_peer_index(m_pNetEvent);
			void *pUserdata;
			EE(ddnet_net_userdata, m_pNet, PeerId, &pUserdata);
			if((uintptr_t)pUserdata == (uintptr_t)-1)
			{
				continue;
			}
			const int ClientId = (uintptr_t)pUserdata;
			dbg_assert(m_aPeers[ClientId].m_Id == PeerId, "invalid peer mapping");
			mem_zero(pChunk, sizeof(*pChunk));
			pChunk->m_ClientId = ClientId;
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
			char aBanReason[256];
			if(NetBan() && NetBan()->IsBanned(&Addr, aBanReason, sizeof(aBanReason)))
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

int CNetServer::Send(CNetChunk *pChunk)
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

	dbg_assert(
		pChunk->m_ClientId >= 0 && pChunk->m_ClientId < MaxClients(),
		"Invalid pChunk->m_ClientId: %d",
		pChunk->m_ClientId);

	const uint64_t PeerId = m_aPeers[pChunk->m_ClientId].m_Id;
	dbg_assert(
		PeerId != (uint64_t)-1,
		"Client not connected");
	EE(ddnet_net_send_chunk, m_pNet, PeerId, (const unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0);
	if((pChunk->m_Flags & NETSENDFLAG_FLUSH) != 0)
	{
		if(m_FlushBatch)
			m_aFlushPending[pChunk->m_ClientId] = true;
		else
			Flush(pChunk->m_ClientId);
	}
	return 0;
}

void CNetServer::SetMaxClientsPerIp(int Max)
{
	m_MaxClientsPerIp = std::clamp<int>(Max, 1, NET_MAX_CLIENTS);
}

bool CNetServer::HasErrored(int ClientId)
{
	return m_aPeers[ClientId].m_State == CPeer::STATE_TIMEOUT ||
	       m_aPeers[ClientId].m_State == CPeer::STATE_TIMEOUT_CLEARED;
}

void CNetServer::ResumeOldConnection(int ClientId, int OrigId)
{
	dbg_assert(HasErrored(ClientId), "client did not time out");
	dbg_assert(m_aPeers[ClientId].m_Id == (uint64_t)-1, "invalid peer id");
	m_aPeers[ClientId] = m_aPeers[OrigId];
	m_aPeers[OrigId].Reset();
	EE(ddnet_net_set_userdata, m_pNet, m_aPeers[ClientId].m_Id, (void *)(uintptr_t)ClientId);
}

void CNetServer::IgnoreTimeouts(int ClientId)
{
	dbg_assert(m_aPeers[ClientId].m_State != CPeer::STATE_NONE, "invalid client id");
	m_aPeers[ClientId].m_TimeoutProtected = true;
}

void CNetServer::ResetErrorString(int ClientId)
{
	dbg_assert(m_aPeers[ClientId].m_State == CPeer::STATE_TIMEOUT, "invalid client state");
	m_aPeers[ClientId].m_State = CPeer::STATE_TIMEOUT_CLEARED;
}

const char *CNetServer::ErrorString(int ClientId)
{
	if(m_aPeers[ClientId].m_State == CPeer::STATE_TIMEOUT)
	{
		return "timeout";
	}
	return "";
}

const NETADDR *CNetServer::ClientAddr(int ClientId) const
{
	dbg_assert(m_aPeers[ClientId].m_State != CPeer::STATE_NONE, "invalid client id");
	return &m_aPeers[ClientId].m_Address;
}

const std::array<char, NETADDR_MAXSTRSIZE> &CNetServer::ClientAddrString(int ClientId, bool IncludePort) const
{
	dbg_assert(m_aPeers[ClientId].m_State != CPeer::STATE_NONE, "invalid client id");
	return IncludePort ? m_aPeers[ClientId].m_aAddressStr : m_aPeers[ClientId].m_aAddressStrNoPort;
}

bool CNetServer::HasSecurityToken(int ClientId) const
{
	// unimplemented
	return true;
}

NETSOCKET CNetServer::Socket() const
{
	// unimplemented
	return nullptr;
}

int CNetServer::NetType() const
{
	// unimplemented
	return NETTYPE_IPV4 | NETTYPE_IPV6;
}
SECURITY_TOKEN CNetServer::GetGlobalToken()
{
	// unimplemented
	return 0xdeadbeef;
}

#endif // CONF_NETWORKING_QUIC
