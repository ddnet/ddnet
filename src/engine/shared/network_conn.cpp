/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "config.h"
#include "network.h"
#include <base/system.h>

void CNetConnection::ResetStats()
{
	mem_zero(&m_Stats, sizeof(m_Stats));
	mem_zero(&m_PeerAddr, sizeof(m_PeerAddr));
	m_aPeerAddrStr[0] = '\0';
	m_LastUpdateTime = 0;
}

void CNetConnection::Reset(bool Rejoin)
{
	m_Sequence = 0;
	m_Ack = 0;
	m_PeerAck = 0;
	m_RemoteClosed = 0;

	if(!Rejoin)
	{
		m_TimeoutProtected = false;
		m_TimeoutSituation = false;

		m_State = NET_CONNSTATE_OFFLINE;
		m_Token = -1;
		m_SecurityToken = NET_SECURITY_TOKEN_UNKNOWN;
		m_Sixup = false;
	}

	m_LastSendTime = 0;
	m_LastRecvTime = 0;
	//m_LastUpdateTime = 0;

	mem_zero(&m_aConnectAddrs, sizeof(m_aConnectAddrs));
	m_NumConnectAddrs = 0;
	//mem_zero(&m_PeerAddr, sizeof(m_PeerAddr));
	m_UnknownSeq = false;

	m_Buffer.Init();

	mem_zero(&m_Construct, sizeof(m_Construct));
}

const char *CNetConnection::ErrorString()
{
	return m_aErrorString;
}

void CNetConnection::SetError(const char *pString)
{
	str_copy(m_aErrorString, pString);
}

void CNetConnection::Init(NETSOCKET Socket, bool BlockCloseMsg)
{
	Reset();
	ResetStats();

	m_Socket = Socket;
	m_BlockCloseMsg = BlockCloseMsg;
	mem_zero(m_aErrorString, sizeof(m_aErrorString));
}

void CNetConnection::AckChunks(int Ack)
{
	while(true)
	{
		CNetChunkResend *pResend = m_Buffer.First();
		if(!pResend)
			break;

		if(CNetBase::IsSeqInBackroom(pResend->m_Sequence, Ack))
			m_Buffer.PopFirst();
		else
			break;
	}
}

void CNetConnection::SignalResend()
{
	m_Construct.m_Flags |= NET_PACKETFLAG_RESEND;
}

int CNetConnection::Flush()
{
	int NumChunks = m_Construct.m_NumChunks;
	if(!NumChunks && !m_Construct.m_Flags)
		return 0;

	// send of the packets
	m_Construct.m_Ack = m_Ack;
	CNetBase::SendPacket(m_Socket, &m_PeerAddr, &m_Construct, m_SecurityToken, m_Sixup);

	// update send times
	m_LastSendTime = time_get();

	// clear construct so we can start building a new package
	mem_zero(&m_Construct, sizeof(m_Construct));
	return NumChunks;
}

int CNetConnection::QueueChunkEx(int Flags, int DataSize, const void *pData, int Sequence)
{
	if(m_State == NET_CONNSTATE_OFFLINE || m_State == NET_CONNSTATE_ERROR)
		return -1;

	unsigned char *pChunkData;

	// check if we have space for it, if not, flush the connection
	if(m_Construct.m_DataSize + DataSize + NET_MAX_CHUNKHEADERSIZE > (int)sizeof(m_Construct.m_aChunkData) - (int)sizeof(SECURITY_TOKEN))
		Flush();

	// pack all the data
	CNetChunkHeader Header;
	Header.m_Flags = Flags;
	Header.m_Size = DataSize;
	Header.m_Sequence = Sequence;
	pChunkData = &m_Construct.m_aChunkData[m_Construct.m_DataSize];
	pChunkData = Header.Pack(pChunkData, m_Sixup ? 6 : 4);
	mem_copy(pChunkData, pData, DataSize);
	pChunkData += DataSize;

	//
	m_Construct.m_NumChunks++;
	m_Construct.m_DataSize = (int)(pChunkData - m_Construct.m_aChunkData);

	// set packet flags as well

	if(Flags & NET_CHUNKFLAG_VITAL && !(Flags & NET_CHUNKFLAG_RESEND))
	{
		// save packet if we need to resend
		CNetChunkResend *pResend = m_Buffer.Allocate(sizeof(CNetChunkResend) + DataSize);
		if(pResend)
		{
			pResend->m_Sequence = Sequence;
			pResend->m_Flags = Flags;
			pResend->m_DataSize = DataSize;
			pResend->m_pData = (unsigned char *)(pResend + 1);
			pResend->m_FirstSendTime = time_get();
			pResend->m_LastSendTime = pResend->m_FirstSendTime;
			mem_copy(pResend->m_pData, pData, DataSize);
		}
		else
		{
			// out of buffer, don't save the packet and hope nobody will ask for resend
			return -1;
		}
	}

	return 0;
}

int CNetConnection::QueueChunk(int Flags, int DataSize, const void *pData)
{
	if(Flags & NET_CHUNKFLAG_VITAL)
		m_Sequence = (m_Sequence + 1) % NET_MAX_SEQUENCE;
	return QueueChunkEx(Flags, DataSize, pData, m_Sequence);
}

void CNetConnection::SendConnect()
{
	// send the connect message
	m_LastSendTime = time_get();
	for(int i = 0; i < m_NumConnectAddrs; i++)
	{
		CNetBase::SendControlMsg(m_Socket, &m_aConnectAddrs[i], m_Ack, NET_CTRLMSG_CONNECT, SECURITY_TOKEN_MAGIC, sizeof(SECURITY_TOKEN_MAGIC), m_SecurityToken, m_Sixup);
	}
}

void CNetConnection::SendControl(int ControlMsg, const void *pExtra, int ExtraSize)
{
	// send the control message
	m_LastSendTime = time_get();
	CNetBase::SendControlMsg(m_Socket, &m_PeerAddr, m_Ack, ControlMsg, pExtra, ExtraSize, m_SecurityToken, m_Sixup);
}

void CNetConnection::ResendChunk(CNetChunkResend *pResend)
{
	QueueChunkEx(pResend->m_Flags | NET_CHUNKFLAG_RESEND, pResend->m_DataSize, pResend->m_pData, pResend->m_Sequence);
	pResend->m_LastSendTime = time_get();
}

void CNetConnection::Resend()
{
	for(CNetChunkResend *pResend = m_Buffer.First(); pResend; pResend = m_Buffer.Next(pResend))
		ResendChunk(pResend);
}

int CNetConnection::Connect(const NETADDR *pAddr, int NumAddrs)
{
	if(State() != NET_CONNSTATE_OFFLINE)
		return -1;

	// init connection
	Reset();
	mem_zero(&m_PeerAddr, sizeof(m_PeerAddr));
	m_aPeerAddrStr[0] = '\0';

	for(int i = 0; i < NumAddrs; i++)
	{
		m_aConnectAddrs[i] = pAddr[i];
	}
	m_NumConnectAddrs = NumAddrs;
	mem_zero(m_aErrorString, sizeof(m_aErrorString));
	m_State = NET_CONNSTATE_CONNECT;
	SendConnect();
	return 0;
}

void CNetConnection::SendControlWithToken7(int ControlMsg, SECURITY_TOKEN ResponseToken)
{
	m_LastSendTime = time_get();

	CNetBase::SendControlMsgWithToken7(m_Socket, &m_PeerAddr, ResponseToken, 0, ControlMsg, m_Token, true);
}

int CNetConnection::Connect7(const NETADDR *pAddr, int NumAddrs)
{
	if(State() != NET_CONNSTATE_OFFLINE)
		return -1;

	// init connection
	Reset();
	mem_zero(&m_PeerAddr, sizeof(m_PeerAddr));
	for(int i = 0; i < NumAddrs; i++)
	{
		m_aConnectAddrs[i] = pAddr[i];
	}
	m_LastRecvTime = time_get();
	m_NumConnectAddrs = NumAddrs;
	m_PeerAddr = *pAddr;
	SetToken7(GenerateToken7(pAddr));
	mem_zero(m_aErrorString, sizeof(m_aErrorString));
	m_State = NET_CONNSTATE_TOKEN;
	SendControlWithToken7(NET_CTRLMSG_TOKEN, NET_TOKEN_NONE);
	m_Sixup = true;
	return 0;
}

void CNetConnection::SetToken7(TOKEN Token)
{
	if(State() != NET_CONNSTATE_OFFLINE)
		return;

	m_Token = Token;
}

TOKEN CNetConnection::GenerateToken7(const NETADDR *pPeerAddr)
{
	TOKEN Token;
	secure_random_fill(&Token, sizeof(Token));
	return Token;
}

void CNetConnection::Disconnect(const char *pReason)
{
	if(State() == NET_CONNSTATE_OFFLINE)
		return;

	if(m_RemoteClosed == 0)
	{
		if(!m_TimeoutSituation)
		{
			if(pReason)
				SendControl(NET_CTRLMSG_CLOSE, pReason, str_length(pReason) + 1);
			else
				SendControl(NET_CTRLMSG_CLOSE, 0, 0);
		}

		if(pReason != m_aErrorString)
		{
			m_aErrorString[0] = 0;
			if(pReason)
				str_copy(m_aErrorString, pReason);
		}
	}

	Reset();
}

void CNetConnection::DirectInit(const NETADDR &Addr, SECURITY_TOKEN SecurityToken, SECURITY_TOKEN Token, bool Sixup)
{
	Reset();

	m_State = NET_CONNSTATE_ONLINE;

	m_PeerAddr = Addr;
	net_addr_str(&Addr, m_aPeerAddrStr, sizeof(m_aPeerAddrStr), true);
	mem_zero(m_aErrorString, sizeof(m_aErrorString));

	int64_t Now = time_get();
	m_LastSendTime = Now;
	m_LastRecvTime = Now;
	m_LastUpdateTime = Now;

	m_SecurityToken = SecurityToken;
	m_Token = Token;
	m_Sixup = Sixup;
}

int CNetConnection::Feed(CNetPacketConstruct *pPacket, NETADDR *pAddr, SECURITY_TOKEN SecurityToken, SECURITY_TOKEN ResponseToken)
{
	// Disregard packets from the wrong address, unless we don't know our peer yet.
	if(State() != NET_CONNSTATE_OFFLINE && State() != NET_CONNSTATE_CONNECT && *pAddr != m_PeerAddr)
	{
		return 0;
	}

	if(!m_Sixup && State() != NET_CONNSTATE_OFFLINE && m_SecurityToken != NET_SECURITY_TOKEN_UNKNOWN && m_SecurityToken != NET_SECURITY_TOKEN_UNSUPPORTED)
	{
		// supposed to have a valid token in this packet, check it
		if(pPacket->m_DataSize < (int)sizeof(m_SecurityToken))
			return 0;
		pPacket->m_DataSize -= sizeof(m_SecurityToken);
		if(m_SecurityToken != ToSecurityToken(&pPacket->m_aChunkData[pPacket->m_DataSize]))
		{
			if(g_Config.m_Debug)
				dbg_msg("security", "token mismatch, expected %d got %d", m_SecurityToken, ToSecurityToken(&pPacket->m_aChunkData[pPacket->m_DataSize]));
			return 0;
		}
	}

	if(m_Sixup && SecurityToken != m_Token)
		return 0;

	// check if actual ack value is valid(own sequence..latest peer ack)
	if(m_Sequence >= m_PeerAck)
	{
		if(pPacket->m_Ack < m_PeerAck || pPacket->m_Ack > m_Sequence)
			return 0;
	}
	else
	{
		if(pPacket->m_Ack < m_PeerAck && pPacket->m_Ack > m_Sequence)
			return 0;
	}
	m_PeerAck = pPacket->m_Ack;

	int64_t Now = time_get();

	// check if resend is requested
	if(pPacket->m_Flags & NET_PACKETFLAG_RESEND)
		Resend();

	//
	if(pPacket->m_Flags & NET_PACKETFLAG_CONTROL)
	{
		int CtrlMsg = pPacket->m_aChunkData[0];

		if(CtrlMsg == NET_CTRLMSG_CLOSE)
		{
			bool IsPeer;
			if(m_State != NET_CONNSTATE_CONNECT)
			{
				IsPeer = m_PeerAddr == *pAddr;
			}
			else
			{
				IsPeer = false;
				for(int i = 0; i < m_NumConnectAddrs; i++)
				{
					if(m_aConnectAddrs[i] == *pAddr)
					{
						IsPeer = true;
						break;
					}
				}
			}
			if(IsPeer)
			{
				m_State = NET_CONNSTATE_ERROR;
				m_RemoteClosed = 1;

				char aStr[256] = {0};
				if(pPacket->m_DataSize > 1)
				{
					// make sure to sanitize the error string from the other party
					str_copy(aStr, (char *)&pPacket->m_aChunkData[1], minimum(pPacket->m_DataSize, (int)sizeof(aStr)));
					str_sanitize_cc(aStr);
				}

				if(!m_BlockCloseMsg)
				{
					// set the error string
					SetError(aStr);
				}

				if(g_Config.m_Debug)
					dbg_msg("conn", "closed reason='%s'", aStr);
			}
			return 0;
		}
		else
		{
			if(CtrlMsg == NET_CTRLMSG_TOKEN)
			{
				if(State() == NET_CONNSTATE_TOKEN)
				{
					m_LastRecvTime = Now;
					m_State = NET_CONNSTATE_CONNECT;
					m_SecurityToken = ResponseToken;
					SendControlWithToken7(NET_CTRLMSG_CONNECT, m_SecurityToken);
					dbg_msg("connection", "got token, replying, token=%x mytoken=%x", m_SecurityToken, m_Token);
				}
				else if(g_Config.m_Debug)
					dbg_msg("connection", "got token, token=%x", ResponseToken);
			}
			else
			{
				if(State() == NET_CONNSTATE_OFFLINE)
				{
					if(CtrlMsg == NET_CTRLMSG_CONNECT)
					{
						if(net_addr_comp_noport(&m_PeerAddr, pAddr) == 0 && time_get() - m_LastUpdateTime < time_freq() * 3)
							return 0;

						// send response and init connection
						Reset();
						m_State = NET_CONNSTATE_PENDING;
						m_PeerAddr = *pAddr;
						net_addr_str(pAddr, m_aPeerAddrStr, sizeof(m_aPeerAddrStr), true);
						mem_zero(m_aErrorString, sizeof(m_aErrorString));
						m_LastSendTime = Now;
						m_LastRecvTime = Now;
						m_LastUpdateTime = Now;
						if(m_SecurityToken == NET_SECURITY_TOKEN_UNKNOWN && pPacket->m_DataSize >= (int)(1 + sizeof(SECURITY_TOKEN_MAGIC) + sizeof(m_SecurityToken)) && !mem_comp(&pPacket->m_aChunkData[1], SECURITY_TOKEN_MAGIC, sizeof(SECURITY_TOKEN_MAGIC)))
						{
							m_SecurityToken = NET_SECURITY_TOKEN_UNSUPPORTED;
							if(g_Config.m_Debug)
								dbg_msg("security", "generated token %d", m_SecurityToken);
						}
						else
						{
							if(g_Config.m_Debug)
								dbg_msg("security", "token not supported by client (packet size %d)", pPacket->m_DataSize);
							m_SecurityToken = NET_SECURITY_TOKEN_UNSUPPORTED;
						}
						SendControl(NET_CTRLMSG_CONNECTACCEPT, SECURITY_TOKEN_MAGIC, sizeof(SECURITY_TOKEN_MAGIC));
						if(g_Config.m_Debug)
							dbg_msg("connection", "got connection, sending connect+accept");
					}
				}
				else if(State() == NET_CONNSTATE_CONNECT)
				{
					// connection made
					if(CtrlMsg == NET_CTRLMSG_CONNECTACCEPT)
					{
						m_PeerAddr = *pAddr;
						net_addr_str(pAddr, m_aPeerAddrStr, sizeof(m_aPeerAddrStr), true);
						if(m_SecurityToken == NET_SECURITY_TOKEN_UNKNOWN && pPacket->m_DataSize >= (int)(1 + sizeof(SECURITY_TOKEN_MAGIC) + sizeof(m_SecurityToken)) && !mem_comp(&pPacket->m_aChunkData[1], SECURITY_TOKEN_MAGIC, sizeof(SECURITY_TOKEN_MAGIC)))
						{
							m_SecurityToken = ToSecurityToken(&pPacket->m_aChunkData[1 + sizeof(SECURITY_TOKEN_MAGIC)]);
							if(g_Config.m_Debug)
								dbg_msg("security", "got token %d", m_SecurityToken);
						}
						else if(!IsSixup())
						{
							m_SecurityToken = NET_SECURITY_TOKEN_UNSUPPORTED;
							if(g_Config.m_Debug)
								dbg_msg("security", "token not supported by server");
						}
						if(!IsSixup())
							SendControl(NET_CTRLMSG_ACCEPT, 0, 0);
						m_LastRecvTime = Now;
						m_State = NET_CONNSTATE_ONLINE;
						if(g_Config.m_Debug)
							dbg_msg("connection", "got connect+accept, sending accept. connection online");
					}
				}
			}
		}
	}
	else
	{
		if(State() == NET_CONNSTATE_PENDING)
		{
			m_LastRecvTime = Now;
			m_State = NET_CONNSTATE_ONLINE;
			if(g_Config.m_Debug)
				dbg_msg("connection", "connecting online");
		}
	}

	if(State() == NET_CONNSTATE_ONLINE)
	{
		m_LastRecvTime = Now;
		AckChunks(pPacket->m_Ack);
	}

	return 1;
}

int CNetConnection::Update()
{
	int64_t Now = time_get();

	if(State() == NET_CONNSTATE_ERROR && m_TimeoutSituation && (Now - m_LastRecvTime) > time_freq() * g_Config.m_ConnTimeoutProtection)
	{
		m_TimeoutSituation = false;
		SetError("Timeout Protection over");
	}

	if(State() == NET_CONNSTATE_OFFLINE || State() == NET_CONNSTATE_ERROR)
		return 0;

	m_TimeoutSituation = false;

	// check for timeout
	if(State() != NET_CONNSTATE_OFFLINE &&
		State() != NET_CONNSTATE_CONNECT &&
		(Now - m_LastRecvTime) > time_freq() * g_Config.m_ConnTimeout)
	{
		m_State = NET_CONNSTATE_ERROR;
		SetError("Timeout");
		m_TimeoutSituation = true;
	}

	// fix resends
	if(m_Buffer.First())
	{
		CNetChunkResend *pResend = m_Buffer.First();

		// check if we have some really old stuff laying around and abort if not acked
		if(Now - pResend->m_FirstSendTime > time_freq() * g_Config.m_ConnTimeout)
		{
			m_State = NET_CONNSTATE_ERROR;
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Too weak connection (not acked for %d seconds)", g_Config.m_ConnTimeout);
			SetError(aBuf);
			m_TimeoutSituation = true;
		}
		else
		{
			// resend packet if we haven't got it acked in 1 second
			if(Now - pResend->m_LastSendTime > time_freq())
				ResendChunk(pResend);
		}
	}

	// send keep alives if nothing has happened for 250ms
	if(State() == NET_CONNSTATE_ONLINE)
	{
		if(time_get() - m_LastSendTime > time_freq() / 2) // flush connection after 500ms if needed
		{
			int NumFlushedChunks = Flush();
			if(NumFlushedChunks && g_Config.m_Debug)
				dbg_msg("connection", "flushed connection due to timeout. %d chunks.", NumFlushedChunks);
		}

		if(time_get() - m_LastSendTime > time_freq())
			SendControl(NET_CTRLMSG_KEEPALIVE, 0, 0);
	}
	else if(State() == NET_CONNSTATE_CONNECT)
	{
		if(time_get() - m_LastSendTime > time_freq() / 2) // send a new connect every 500ms
			SendConnect();
	}
	else if(State() == NET_CONNSTATE_PENDING)
	{
		if(time_get() - m_LastSendTime > time_freq() / 2) // send a new connect/accept every 500ms
			SendControl(NET_CTRLMSG_CONNECTACCEPT, SECURITY_TOKEN_MAGIC, sizeof(SECURITY_TOKEN_MAGIC));
	}

	return 0;
}

void CNetConnection::SetTimedOut(const NETADDR *pAddr, int Sequence, int Ack, SECURITY_TOKEN SecurityToken, CStaticRingBuffer<CNetChunkResend, NET_CONN_BUFFERSIZE> *pResendBuffer, bool Sixup)
{
	int64_t Now = time_get();

	m_Sequence = Sequence;
	m_Ack = Ack;
	m_RemoteClosed = 0;

	m_State = NET_CONNSTATE_ONLINE;
	m_PeerAddr = *pAddr;
	net_addr_str(pAddr, m_aPeerAddrStr, sizeof(m_aPeerAddrStr), true);
	mem_zero(m_aErrorString, sizeof(m_aErrorString));
	m_LastSendTime = Now;
	m_LastRecvTime = Now;
	m_LastUpdateTime = Now;
	m_SecurityToken = SecurityToken;
	m_Sixup = Sixup;

	// copy resend buffer
	m_Buffer.Init();
	while(pResendBuffer->First())
	{
		CNetChunkResend *pFirst = pResendBuffer->First();

		CNetChunkResend *pResend = m_Buffer.Allocate(sizeof(CNetChunkResend) + pFirst->m_DataSize);
		mem_copy(pResend, pFirst, sizeof(CNetChunkResend) + pFirst->m_DataSize);

		pResendBuffer->PopFirst();
	}
}
