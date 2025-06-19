/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "network.h"
#include <base/system.h>
#include <base/types.h>

bool CNetClient::Open(NETADDR BindAddr)
{
	// open socket
	NETSOCKET Socket;
	Socket = net_udp_create(BindAddr);
	if(!Socket)
		return false;

	// clean it
	*this = CNetClient{};

	// init
	m_Socket = Socket;
	m_pStun = new CStun(m_Socket);
	m_Connection.Init(m_Socket, false);
	m_TokenCache.Init(m_Socket);

	return true;
}

void CNetClient::Close()
{
	if(!m_Socket)
	{
		return;
	}
	if(m_pStun)
	{
		delete m_pStun;
		m_pStun = nullptr;
	}
	net_udp_close(m_Socket);
	m_Socket = nullptr;
}

void CNetClient::Disconnect(const char *pReason)
{
	m_Connection.Disconnect(pReason);
}

void CNetClient::Update()
{
	m_Connection.Update();
	if(m_Connection.State() == NET_CONNSTATE_ERROR)
		Disconnect(m_Connection.ErrorString());
	m_pStun->Update();
	m_TokenCache.Update();
}

void CNetClient::Connect(const NETADDR *pAddr, int NumAddrs)
{
	m_Connection.Connect(pAddr, NumAddrs);
}

void CNetClient::Connect7(const NETADDR *pAddr, int NumAddrs)
{
	m_Connection.Connect7(pAddr, NumAddrs);
}

void CNetClient::ResetErrorString()
{
	m_Connection.ResetErrorString();
}

int CNetClient::Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken, bool Sixup)
{
	while(true)
	{
		// check for a chunk
		if(m_RecvUnpacker.FetchChunk(pChunk))
			return 1;

		// TODO: empty the recvinfo
		NETADDR Addr;
		unsigned char *pData;
		int Bytes = net_udp_recv(m_Socket, &Addr, &pData);

		// no more packets for now
		if(Bytes <= 0)
			break;

		if(m_pStun->OnPacket(Addr, pData, Bytes))
		{
			continue;
		}

		SECURITY_TOKEN Token;
		*pResponseToken = NET_SECURITY_TOKEN_UNKNOWN;
		if(CNetBase::UnpackPacket(pData, Bytes, &m_RecvUnpacker.m_Data, Sixup, &Token, pResponseToken) == 0)
		{
			if(Sixup)
				Addr.type |= NETTYPE_TW7;
			if(m_RecvUnpacker.m_Data.m_aChunkData[0] == NET_CTRLMSG_TOKEN)
			{
				m_TokenCache.AddToken(&Addr, *pResponseToken);
			}
			if(m_RecvUnpacker.m_Data.m_Flags & NET_PACKETFLAG_CONNLESS)
			{
				pChunk->m_Flags = NETSENDFLAG_CONNLESS;
				pChunk->m_ClientId = -1;
				pChunk->m_Address = Addr;
				pChunk->m_DataSize = m_RecvUnpacker.m_Data.m_DataSize;
				pChunk->m_pData = m_RecvUnpacker.m_Data.m_aChunkData;
				if(m_RecvUnpacker.m_Data.m_Flags & NET_PACKETFLAG_EXTENDED)
				{
					pChunk->m_Flags |= NETSENDFLAG_EXTENDED;
					mem_copy(pChunk->m_aExtraData, m_RecvUnpacker.m_Data.m_aExtraData, sizeof(pChunk->m_aExtraData));
				}
				return 1;
			}
			else
			{
				if(m_Connection.State() != NET_CONNSTATE_OFFLINE && m_Connection.State() != NET_CONNSTATE_ERROR && m_Connection.Feed(&m_RecvUnpacker.m_Data, &Addr, Token, *pResponseToken))
					m_RecvUnpacker.Start(&Addr, &m_Connection, 0);
			}
		}
	}
	return 0;
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
		// send connectionless packet
		if(pChunk->m_Address.type & NETTYPE_TW7)
		{
			m_TokenCache.SendPacketConnless(pChunk);
		}
		else
		{
			CNetBase::SendPacketConnless(m_Socket, &pChunk->m_Address, pChunk->m_pData, pChunk->m_DataSize,
				pChunk->m_Flags & NETSENDFLAG_EXTENDED, pChunk->m_aExtraData);
		}
	}
	else
	{
		int Flags = 0;
		dbg_assert(pChunk->m_ClientId == 0, "erroneous client id");

		if(pChunk->m_Flags & NETSENDFLAG_VITAL)
			Flags = NET_CHUNKFLAG_VITAL;

		m_Connection.QueueChunk(Flags, pChunk->m_DataSize, pChunk->m_pData);

		if(pChunk->m_Flags & NETSENDFLAG_FLUSH)
			m_Connection.Flush();
	}
	return 0;
}

int CNetClient::State()
{
	if(m_Connection.State() == NET_CONNSTATE_ONLINE)
		return NETSTATE_ONLINE;
	if(m_Connection.State() == NET_CONNSTATE_OFFLINE)
		return NETSTATE_OFFLINE;
	return NETSTATE_CONNECTING;
}

int CNetClient::Flush()
{
	return m_Connection.Flush();
}

bool CNetClient::GotProblems(int64_t MaxLatency) const
{
	return time_get() - m_Connection.LastRecvTime() > MaxLatency;
}

const char *CNetClient::ErrorString() const
{
	return m_Connection.ErrorString();
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
