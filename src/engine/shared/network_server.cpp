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
	for(auto &Peer : m_aPeerMapping)
	{
		Peer = -1;
	}

	// TODO: use the actual bind address, not just the port
	char aBindAddr[NETADDR_MAXSTRSIZE];
	str_format(aBindAddr, sizeof(aBindAddr), "0.0.0.0:%d", BindAddr.port);

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
	return 0;
}

int CNetServer::Drop(int ClientID, const char *pReason)
{
	if(m_pfnDelClient)
	{
		m_pfnDelClient(ClientID, pReason, m_pUser);
	}
	uint64_t PeerID = m_aPeerMapping[ClientID];
	dbg_assert(m_aPeerMapping[ClientID] != (uint64_t)-1, "invalid client id");

	// Reset peer mapping.
	m_aPeerMapping[ClientID] = -1;
	if(ddnet_net_set_userdata(m_pNet, PeerID, (void *)(uintptr_t)-1))
	{
		log_error("net", "couldn't set userdata: %s", ddnet_net_error(m_pNet));
		exit(1);
	}

	// Close the connection.
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
		uint64_t PeerID;
		DdnetNetEvent Event;
		// Keep space for null termination.
		if(ddnet_net_recv(m_pNet, m_aBuffer, sizeof(m_aBuffer) - 1, &PeerID, &Event))
		{
			log_error("net", "recv failed: %s", ddnet_net_error(m_pNet));
			exit(1);
		}
		int ClientID;
		void *pUserdata;
		switch(ddnet_net_ev_kind(&Event))
		{
		case DDNET_NET_EV_NONE:
			return 0;
		case DDNET_NET_EV_CONNECT:
			for(int i = 0; i < NET_MAX_CLIENTS; i++)
			{
				if(m_aPeerMapping[m_NextClientID] == (uint64_t)-1)
				{
					break;
				}
				m_NextClientID = (m_NextClientID + 1) % NET_MAX_CLIENTS;
			}
			ClientID = m_NextClientID;
			m_NextClientID = (m_NextClientID + 1) % NET_MAX_CLIENTS;
			if(m_aPeerMapping[ClientID] != (uint64_t)-1)
			{
				static const char FULL[] = "This server is full";
				if(ddnet_net_close(m_pNet, PeerID, FULL, sizeof(FULL) - 1))
				{
					log_error("net", "drop failed: %s", ddnet_net_error(m_pNet));
					exit(1);
				}
			}
			m_aPeerMapping[ClientID] = PeerID;
			if(ddnet_net_set_userdata(m_pNet, PeerID, (void *)(uintptr_t)ClientID))
			{
				log_error("net", "couldn't set userdata: %s", ddnet_net_error(m_pNet));
				exit(1);
			}
			if(m_pfnNewClient)
			{
				m_pfnNewClient(ClientID, m_pUser, false);
			}
			break;
		case DDNET_NET_EV_DISCONNECT:
			// TODO: can a disconnect happen before a connect?
			if(ddnet_net_userdata(m_pNet, PeerID, &pUserdata))
			{
				log_error("net", "couldn't get userdata: %s", ddnet_net_error(m_pNet));
				exit(1);
			}
			if((uintptr_t)pUserdata == (uintptr_t)-1)
			{
				continue;
			}
			ClientID = (uintptr_t)pUserdata;
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
				log_debug("net", "reason len: %d", (int)ddnet_net_ev_disconnect_reason_len(&Event));
				m_aBuffer[ddnet_net_ev_disconnect_reason_len(&Event)] = 0;
				const char *pReason = ddnet_net_ev_disconnect_is_remote(&Event) ? "" : (char *)m_aBuffer;
				m_pfnDelClient(ClientID, pReason, m_pUser);
			}
			dbg_assert(m_aPeerMapping[ClientID] == PeerID, "invalid peer mapping");
			m_aPeerMapping[ClientID] = -1;
			break;
		case DDNET_NET_EV_CHUNK:
			if(ddnet_net_userdata(m_pNet, PeerID, &pUserdata))
			{
				log_error("net", "couldn't get userdata: %s", ddnet_net_error(m_pNet));
				exit(1);
			}
			ClientID = (uintptr_t)pUserdata;
			dbg_assert(m_aPeerMapping[ClientID] == PeerID, "invalid peer mapping");
			log_debug("net", "chunk len=%d", (int)ddnet_net_ev_chunk_len(&Event));
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
		uint64_t PeerID = m_aPeerMapping[pChunk->m_ClientID];
		dbg_assert(m_aPeerMapping[pChunk->m_ClientID] != (uint64_t)-1, "invalid client id");
		if(ddnet_net_send_chunk(m_pNet, PeerID, (unsigned char *)pChunk->m_pData, pChunk->m_DataSize, (pChunk->m_Flags & NETSENDFLAG_VITAL) == 0))
		{
			log_error("net", "send failed: %s", ddnet_net_error(m_pNet));
			exit(1);
		}
		if((pChunk->m_Flags & NETSENDFLAG_FLUSH) != 0)
		{
			if(ddnet_net_flush(m_pNet, PeerID))
			{
				log_error("net", "flush failed: %s", ddnet_net_error(m_pNet));
				exit(1);
			}
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
