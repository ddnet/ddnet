#include "network.h"

#include "netban.h"
#include <base/log.h>
#include <engine/shared/config.h>
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

static unsigned char IDENTITY[] = {
	0x5c, 0xf2, 0xa4, 0xf0, 0xed, 0x3d, 0xc8, 0x5b, 0x3f, 0x4b, 0xfa, 0x5c,
	0xa9, 0x7b, 0x8a, 0xde, 0xaf, 0x0d, 0x5e, 0xc1, 0x65, 0x17, 0x9a, 0xf8,
	0x05, 0xa3, 0xf7, 0x75, 0x1d, 0xd8, 0xac, 0x47
};

static bool AddrFromUrl(const char *pUrl, NETADDR *pAddr)
{
	// TODO: maybe parse URL by ourselves
	CURLU *pHandle = curl_url();
	if(curl_url_set(pHandle, CURLUPART_URL, pUrl, CURLU_NON_SUPPORT_SCHEME))
	{
		curl_url_cleanup(pHandle);
		return false;
	}
	char *pHostname;
	if(curl_url_get(pHandle, CURLUPART_HOST, &pHostname, 0))
	{
		curl_url_cleanup(pHandle);
		return false;
	}
	curl_url_cleanup(pHandle);
	if(net_addr_from_str(pAddr, pHostname))
	{
		return false;
	}
	return true;
}

void CNetServer::CPeer::Reset()
{
	m_State = STATE_NONE;
	m_ID = -1;
	m_TimeoutProtected = false;
	mem_zero(&m_Address, sizeof(m_Address));
}

CNetServer::~CNetServer()
{
	Close();
}

bool CNetServer::Open(NETADDR BindAddr, CNetBan *pNetBan, int MaxClients, int MaxClientsPerIP)
{
	m_pNetBan = pNetBan;

	for(auto &Peer : m_aPeers)
	{
		Peer.Reset();
	}

	// TODO: use the actual bind address, not just the port
	char aBindAddr[NETADDR_MAXSTRSIZE];
	str_format(aBindAddr, sizeof(aBindAddr), "0.0.0.0:%d", BindAddr.port);

	ddnet_net_ev_new(&m_pNetEvent);
	if(false
		|| ddnet_net_new(&m_pNet)
		|| ddnet_net_set_bindaddr(m_pNet, aBindAddr, str_length(aBindAddr))
		|| ddnet_net_set_identity(m_pNet, &IDENTITY)
		|| ddnet_net_set_accept_connections(m_pNet, true)
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
	if(m_pNetEvent)
	{
		ddnet_net_ev_free(m_pNetEvent);
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
	uint64_t PeerID = m_aPeers[ClientID].m_ID;
	dbg_assert(m_aPeers[ClientID].m_ID != (uint64_t)-1, "invalid client id");

	// Reset peer mapping.
	m_aPeers[ClientID].m_ID = -1;
	EE(ddnet_net_set_userdata, m_pNet, PeerID, (void *)(uintptr_t)-1);

	// Close the connection.
	EE(ddnet_net_close, m_pNet, ClientID, pReason, str_length(pReason));
	return 0;
}

int CNetServer::Update()
{
	// TODO: call timeout stuff
	return 0;
}

void CNetServer::Wait(uint64_t Microseconds)
{
	EE(ddnet_net_wait_timeout, m_pNet, Microseconds * 1000);
}

int CNetServer::Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken)
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
				for(int i = 0; i < NET_MAX_CLIENTS; i++)
				{
					if(m_aPeers[m_NextClientID].m_ID == (uint64_t)-1)
					{
						break;
					}
					m_NextClientID = (m_NextClientID + 1) % NET_MAX_CLIENTS;
				}
				int ClientID = m_NextClientID;
				m_NextClientID = (m_NextClientID + 1) % NET_MAX_CLIENTS;
				if(m_aPeers[ClientID].m_ID != (uint64_t)-1)
				{
					static const char FULL[] = "This server is full";
					EE(ddnet_net_close, m_pNet, PeerID, FULL, sizeof(FULL) - 1);
				}
				const char *pAddr;
				size_t AddrLen;
				ddnet_net_ev_connect_addr(m_pNetEvent, &pAddr, &AddrLen);
				NETADDR Addr;
				if(!AddrFromUrl(pAddr, &Addr))
				{
					static const char UNRECOGNIZED_ADDR[] = "Unrecognized address";
					EE(ddnet_net_close, m_pNet, PeerID, UNRECOGNIZED_ADDR, sizeof(UNRECOGNIZED_ADDR) - 1);
				}

				char aBanReason[256];
				if(m_pNetBan->IsBanned(&Addr, aBanReason, sizeof(aBanReason)))
				{
					EE(ddnet_net_close, m_pNet, PeerID, aBanReason, str_length(aBanReason));
				}

				uint32_t NumConnected;
				EE(ddnet_net_num_peers_in_bucket, m_pNet, pAddr, AddrLen, &NumConnected);
				if((int)NumConnected > g_Config.m_SvMaxClientsPerIP)
				{
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Only %d players with the same IP are allowed", g_Config.m_SvMaxClientsPerIP);
					EE(ddnet_net_close, m_pNet, PeerID, aBuf, str_length(aBuf));
				}

				m_aPeers[ClientID].m_State = CPeer::STATE_CONNECTED;
				m_aPeers[ClientID].m_ID = PeerID;
				m_aPeers[ClientID].m_Address = Addr;
				EE(ddnet_net_set_userdata, m_pNet, PeerID, (void *)(uintptr_t)ClientID);
				if(m_pfnNewClient)
				{
					m_pfnNewClient(ClientID, m_pUser, false);
				}
			}
			break;
		case DDNET_NET_EV_DISCONNECT:
			{
				uint64_t PeerID = ddnet_net_ev_disconnect_peer_index(m_pNetEvent);
				// TODO: can a disconnect happen before a connect?
				void *pUserdata;
				EE(ddnet_net_userdata, m_pNet, PeerID, &pUserdata);
				if((uintptr_t)pUserdata == (uintptr_t)-1)
				{
					continue;
				}
				int ClientID = (uintptr_t)pUserdata;
				// TODO: should we disallow sending packets in the
				// `m_pfnDelClient` callback? currently some are sent
				// #1  0x00005555555ee6a7 in dbg_assert_imp (
				//     filename=0x5555557c3300 "src/engine/shared/network_server.cpp", line=219,
				//     test=0, msg=0x5555557c32e8 "invalid client id") at src/base/system.cpp:188
				// #2  0x00005555555d398f in CNetServer::Send (this=0x7ffff5c18490, pChunk=0x7fffffffadc0)
				//     at src/engine/shared/network_server.cpp:219
				// #3  0x0000555555629934 in CServer::SendMsg (this=0x7ffff552d010, pMsg=0x7fffffffbe80, Flags=1,
				//     ClientID=0) at src/engine/server/server.cpp:859
				// #4  0x000055555565b406 in IServer::SendPackMsgOne<CNetMsg_Sv_KillMsg> (this=0x7ffff552d010,
				//     pMsg=0x7fffffffc6e0, Flags=1, ClientID=0) at src/engine/server.h:167
				// #5  0x000055555565a4a8 in IServer::SendPackMsgTranslate (this=0x7ffff552d010,
				//     pMsg=0x7fffffffc770, Flags=1, ClientID=0) at src/engine/server.h:156
				// #6  0x000055555565b613 in IServer::SendPackMsg<CNetMsg_Sv_KillMsg, 0> (this=0x7ffff552d010,
				//     pMsg=0x7fffffffc770, Flags=1, ClientID=-1) at src/engine/server.h:84
				// #7  0x000055555565114c in CCharacter::Die (this=0x555555932e40 <gs_PoolDataCCharacter>, Killer=0,
				//     Weapon=-3) at src/game/server/entities/character.cpp:928
				// #8  0x0000555555699632 in CPlayer::KillCharacter (this=0x555555980d20 <gs_PoolDataCPlayer>,
				//     Weapon=-3) at src/game/server/player.cpp:566
				// #9  0x0000555555699116 in CPlayer::OnDisconnect (this=0x555555980d20 <gs_PoolDataCPlayer>)
				//     at src/game/server/player.cpp:489
				// #10 0x000055555568b4a0 in IGameController::OnPlayerDisconnect (this=0x555555a0ed70,
				//     pPlayer=0x555555980d20 <gs_PoolDataCPlayer>, pReason=0x5555557c33e2 "")
				//     at src/game/server/gamecontroller.cpp:407
				// #11 0x0000555555691669 in CGameControllerDDRace::OnPlayerDisconnect (this=0x555555a0ed70,
				//     pPlayer=0x555555980d20 <gs_PoolDataCPlayer>, pReason=0x5555557c33e2 "")
				//     at src/game/server/gamemodes/DDRace.cpp:153
				// #12 0x000055555566fb1b in CGameContext::OnClientDrop (this=0x7ffff44ea010, ClientID=0,
				//     pReason=0x5555557c33e2 "") at src/game/server/gamecontext.cpp:1619
				// #13 0x000055555562af69 in CServer::DelClientCallback (ClientID=0, pReason=0x5555557c33e2 "",
				//     pUser=0x7ffff552d010) at src/engine/server/server.cpp:1151

				if(m_pfnDelClient)
				{
					m_aBuffer[ddnet_net_ev_disconnect_reason_len(m_pNetEvent)] = 0;
					const char *pReason = ddnet_net_ev_disconnect_is_remote(m_pNetEvent) ? "" : (char *)m_aBuffer;
					m_pfnDelClient(ClientID, pReason, m_pUser);
				}
				dbg_assert(m_aPeers[ClientID].m_ID == PeerID, "invalid peer mapping");
				m_aPeers[ClientID].m_ID = -1;
				m_aPeers[ClientID].m_State = CPeer::STATE_NONE;
			}
			break;
		case DDNET_NET_EV_CHUNK:
			{
				uint64_t PeerID = ddnet_net_ev_chunk_peer_index(m_pNetEvent);
				void *pUserdata;
				EE(ddnet_net_userdata, m_pNet, PeerID, &pUserdata);
				int ClientID = (uintptr_t)pUserdata;
				dbg_assert(m_aPeers[ClientID].m_ID == PeerID, "invalid peer mapping");
				log_debug("net", "chunk len=%d", (int)ddnet_net_ev_chunk_len(m_pNetEvent));
				mem_zero(pChunk, sizeof(*pChunk));
				pChunk->m_ClientID = ClientID;
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
			continue;
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
		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&pChunk->m_Address, aAddr, sizeof(aAddr), true);
		char aUrl[128];
		str_format(aUrl, sizeof(aUrl), "tw-0.6+udp://%s", aAddr);
		EE(ddnet_net_send_connless_chunk, m_pNet, aAddr, str_length(aAddr), (const unsigned char *)pChunk->m_pData, pChunk->m_DataSize);
		return 0;
	}
	else
	{
		dbg_assert(pChunk->m_ClientID >= 0, "erroneous client id");
		dbg_assert(pChunk->m_ClientID < MaxClients(), "erroneous client id");
		uint64_t PeerID = m_aPeers[pChunk->m_ClientID].m_ID;
		dbg_assert(m_aPeers[pChunk->m_ClientID].m_ID != (uint64_t)-1, "invalid client id");
		EE(ddnet_net_send_chunk, m_pNet, PeerID, (const unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0);
		if((pChunk->m_Flags & NETSENDFLAG_FLUSH) != 0)
		{
			EE(ddnet_net_flush, m_pNet, PeerID);
		}
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
	dbg_assert(m_aPeers[ClientID].m_State != CPeer::STATE_NONE, "invalid client id");
	if(m_aPeers[ClientID].m_State != CPeer::STATE_TIMEOUT && m_aPeers[ClientID].m_State != CPeer::STATE_TIMEOUT_CLEARED)
	{
		return false;
	}
	dbg_assert(m_aPeers[ClientID].m_ID == (uint64_t)-1, "invalid peer id");
	m_aPeers[ClientID] = m_aPeers[OrigID];
	m_aPeers[OrigID].Reset();
	EE(ddnet_net_set_userdata, m_pNet, m_aPeers[ClientID].m_ID, (void *)(uintptr_t)ClientID);
	return true;
}

void CNetServer::SetTimeoutProtected(int ClientID)
{
	dbg_assert(m_aPeers[ClientID].m_State != CPeer::STATE_NONE, "invalid client id");
	m_aPeers[ClientID].m_TimeoutProtected = true;
}

int CNetServer::ResetErrorString(int ClientID)
{
	dbg_assert(m_aPeers[ClientID].m_State != CPeer::STATE_NONE, "invalid client id");
	dbg_assert(m_aPeers[ClientID].m_State == CPeer::STATE_TIMEOUT, "invalid client state");
	m_aPeers[ClientID].m_State = CPeer::STATE_TIMEOUT_CLEARED;
	return 0;
}

const char *CNetServer::ErrorString(int ClientID)
{
	dbg_assert(m_aPeers[ClientID].m_State != CPeer::STATE_NONE, "invalid client id");
	if(m_aPeers[ClientID].m_State == CPeer::STATE_TIMEOUT)
	{
		return "timeout";
	}
	else
	{
		return "";
	}
}

const NETADDR *CNetServer::ClientAddr(int ClientID) const
{
	dbg_assert(m_aPeers[ClientID].m_State != CPeer::STATE_NONE, "invalid client id");
	return &m_aPeers[ClientID].m_Address;
}

CNetBan *CNetServer::NetBan() const
{
	return m_pNetBan;
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
