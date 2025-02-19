/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/types.h>

#include "config.h"
#include "huffman.h"
#include "network.h"

const unsigned char SECURITY_TOKEN_MAGIC[4] = {'T', 'K', 'E', 'N'};

SECURITY_TOKEN ToSecurityToken(const unsigned char *pData)
{
	return bytes_be_to_uint(pData);
}

void WriteSecurityToken(unsigned char *pData, SECURITY_TOKEN Token)
{
	uint_to_bytes_be(pData, Token);
}

void CNetRecvUnpacker::Clear()
{
	m_Valid = false;
}

void CNetRecvUnpacker::Start(const NETADDR *pAddr, CNetConnection *pConnection, int ClientId)
{
	m_Addr = *pAddr;
	m_pConnection = pConnection;
	m_ClientId = ClientId;
	m_CurrentChunk = 0;
	m_Valid = true;
}

// TODO: rename this function
int CNetRecvUnpacker::FetchChunk(CNetChunk *pChunk)
{
	CNetChunkHeader Header;
	unsigned char *pEnd = m_Data.m_aChunkData + m_Data.m_DataSize;

	while(true)
	{
		unsigned char *pData = m_Data.m_aChunkData;

		// check for old data to unpack
		if(!m_Valid || m_CurrentChunk >= m_Data.m_NumChunks)
		{
			Clear();
			return 0;
		}

		// TODO: add checking here so we don't read too far
		for(int i = 0; i < m_CurrentChunk; i++)
		{
			pData = Header.Unpack(pData, (m_pConnection && m_pConnection->m_Sixup) ? 6 : 4);
			pData += Header.m_Size;
		}

		// unpack the header
		pData = Header.Unpack(pData, (m_pConnection && m_pConnection->m_Sixup) ? 6 : 4);
		m_CurrentChunk++;

		if(pData + Header.m_Size > pEnd)
		{
			Clear();
			return 0;
		}

		// handle sequence stuff
		if(m_pConnection && (Header.m_Flags & NET_CHUNKFLAG_VITAL))
		{
			// anti spoof: ignore unknown sequence
			if(Header.m_Sequence == (m_pConnection->m_Ack + 1) % NET_MAX_SEQUENCE || m_pConnection->m_UnknownSeq)
			{
				m_pConnection->m_UnknownSeq = false;

				// in sequence
				m_pConnection->m_Ack = Header.m_Sequence;
			}
			else
			{
				// old packet that we already got
				if(CNetBase::IsSeqInBackroom(Header.m_Sequence, m_pConnection->m_Ack))
					continue;

				// out of sequence, request resend
				if(g_Config.m_Debug)
					dbg_msg("conn", "asking for resend %d %d", Header.m_Sequence, (m_pConnection->m_Ack + 1) % NET_MAX_SEQUENCE);
				m_pConnection->SignalResend();
				continue; // take the next chunk in the packet
			}
		}

		// fill in the info
		pChunk->m_ClientId = m_ClientId;
		pChunk->m_Address = m_Addr;
		pChunk->m_Flags = Header.m_Flags;
		pChunk->m_DataSize = Header.m_Size;
		pChunk->m_pData = pData;
		return 1;
	}
}

static const unsigned char NET_HEADER_EXTENDED[] = {'x', 'e'};
// packs the data tight and sends it
void CNetBase::SendPacketConnless(NETSOCKET Socket, NETADDR *pAddr, const void *pData, int DataSize, bool Extended, unsigned char aExtra[4])
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE];
	const int DATA_OFFSET = 6;
	if(!Extended)
	{
		for(int i = 0; i < DATA_OFFSET; i++)
			aBuffer[i] = 0xff;
	}
	else
	{
		mem_copy(aBuffer, NET_HEADER_EXTENDED, sizeof(NET_HEADER_EXTENDED));
		mem_copy(aBuffer + sizeof(NET_HEADER_EXTENDED), aExtra, 4);
	}
	mem_copy(aBuffer + DATA_OFFSET, pData, DataSize);
	net_udp_send(Socket, pAddr, aBuffer, DataSize + DATA_OFFSET);
}

void CNetBase::SendPacketConnlessWithToken7(NETSOCKET Socket, NETADDR *pAddr, const void *pData, int DataSize, SECURITY_TOKEN Token, SECURITY_TOKEN ResponseToken)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE];
	aBuffer[0] = (NET_PACKETFLAG_CONNLESS << 2) | 1;

	const int DATA_OFFSET = 9;

	WriteSecurityToken(aBuffer + 1, Token);
	WriteSecurityToken(aBuffer + 5, ResponseToken);
	mem_copy(aBuffer + DATA_OFFSET, pData, DataSize);
	net_udp_send(Socket, pAddr, aBuffer, DataSize + DATA_OFFSET);
}

void CNetBase::SendPacket(NETSOCKET Socket, NETADDR *pAddr, CNetPacketConstruct *pPacket, SECURITY_TOKEN SecurityToken, bool Sixup, bool NoCompress)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE];
	int CompressedSize = -1;
	int FinalSize = -1;

	// log the data
	if(ms_DataLogSent)
	{
		int Type = 1;
		io_write(ms_DataLogSent, &Type, sizeof(Type));
		io_write(ms_DataLogSent, &pPacket->m_DataSize, sizeof(pPacket->m_DataSize));
		io_write(ms_DataLogSent, &pPacket->m_aChunkData, pPacket->m_DataSize);
		io_flush(ms_DataLogSent);
	}

	int HeaderSize = NET_PACKETHEADERSIZE;
	if(Sixup)
	{
		HeaderSize += sizeof(SecurityToken);
		WriteSecurityToken(aBuffer + 3, SecurityToken);
	}
	else if(SecurityToken != NET_SECURITY_TOKEN_UNSUPPORTED)
	{
		// append security token
		// if SecurityToken is NET_SECURITY_TOKEN_UNKNOWN we will still append it hoping to negotiate it
		WriteSecurityToken(pPacket->m_aChunkData + pPacket->m_DataSize, SecurityToken);
		pPacket->m_DataSize += sizeof(SecurityToken);
	}

	// compress
	if(!NoCompress)
		CompressedSize = ms_Huffman.Compress(pPacket->m_aChunkData, pPacket->m_DataSize, &aBuffer[HeaderSize], NET_MAX_PACKETSIZE - HeaderSize);

	// check if the compression was enabled, successful and good enough
	if(!NoCompress && CompressedSize > 0 && CompressedSize < pPacket->m_DataSize)
	{
		FinalSize = CompressedSize;
		pPacket->m_Flags |= NET_PACKETFLAG_COMPRESSION;
	}
	else
	{
		// use uncompressed data
		FinalSize = pPacket->m_DataSize;
		mem_copy(&aBuffer[HeaderSize], pPacket->m_aChunkData, pPacket->m_DataSize);
		pPacket->m_Flags &= ~NET_PACKETFLAG_COMPRESSION;
	}

	if(Sixup)
	{
		unsigned Flags = 0;
		if(pPacket->m_Flags & NET_PACKETFLAG_CONTROL)
			Flags |= 1;
		if(pPacket->m_Flags & NET_PACKETFLAG_RESEND)
			Flags |= 2;
		if(pPacket->m_Flags & NET_PACKETFLAG_COMPRESSION)
			Flags |= 4;
		pPacket->m_Flags = Flags;
	}

	// set header and send the packet if all things are good
	if(FinalSize >= 0)
	{
		FinalSize += HeaderSize;
		aBuffer[0] = ((pPacket->m_Flags << 2) & 0xfc) | ((pPacket->m_Ack >> 8) & 0x3);
		aBuffer[1] = pPacket->m_Ack & 0xff;
		aBuffer[2] = pPacket->m_NumChunks;
		net_udp_send(Socket, pAddr, aBuffer, FinalSize);

		// log raw socket data
		if(ms_DataLogSent)
		{
			int Type = 0;
			io_write(ms_DataLogSent, &Type, sizeof(Type));
			io_write(ms_DataLogSent, &FinalSize, sizeof(FinalSize));
			io_write(ms_DataLogSent, aBuffer, FinalSize);
			io_flush(ms_DataLogSent);
		}
	}
}

// TODO: rename this function
int CNetBase::UnpackPacket(unsigned char *pBuffer, int Size, CNetPacketConstruct *pPacket, bool &Sixup, SECURITY_TOKEN *pSecurityToken, SECURITY_TOKEN *pResponseToken)
{
	// check the size
	if(Size < NET_PACKETHEADERSIZE || Size > NET_MAX_PACKETSIZE)
		return -1;

	// log the data
	if(ms_DataLogRecv)
	{
		int Type = 0;
		io_write(ms_DataLogRecv, &Type, sizeof(Type));
		io_write(ms_DataLogRecv, &Size, sizeof(Size));
		io_write(ms_DataLogRecv, pBuffer, Size);
		io_flush(ms_DataLogRecv);
	}

	// read the packet
	pPacket->m_Flags = pBuffer[0] >> 2;

	if(pPacket->m_Flags & NET_PACKETFLAG_CONNLESS)
	{
		Sixup = (pBuffer[0] & 0x3) == 1;
		if(Sixup && (pSecurityToken == nullptr || pResponseToken == nullptr))
			return -1;
		int Offset = Sixup ? 9 : 6;
		if(Size < Offset)
			return -1;

		if(Sixup)
		{
			*pSecurityToken = ToSecurityToken(pBuffer + 1);
			*pResponseToken = ToSecurityToken(pBuffer + 5);
		}

		pPacket->m_Flags = NET_PACKETFLAG_CONNLESS;
		pPacket->m_Ack = 0;
		pPacket->m_NumChunks = 0;
		pPacket->m_DataSize = Size - Offset;
		mem_copy(pPacket->m_aChunkData, pBuffer + Offset, pPacket->m_DataSize);

		if(!Sixup && mem_comp(pBuffer, NET_HEADER_EXTENDED, sizeof(NET_HEADER_EXTENDED)) == 0)
		{
			pPacket->m_Flags |= NET_PACKETFLAG_EXTENDED;
			mem_copy(pPacket->m_aExtraData, pBuffer + sizeof(NET_HEADER_EXTENDED), sizeof(pPacket->m_aExtraData));
		}
	}
	else
	{
		if(pPacket->m_Flags & NET_PACKETFLAG_UNUSED)
			Sixup = true;
		if(Sixup && pSecurityToken == nullptr)
			return -1;
		int DataStart = Sixup ? 7 : NET_PACKETHEADERSIZE;
		if(Size < DataStart)
			return -1;

		pPacket->m_Ack = ((pBuffer[0] & 0x3) << 8) | pBuffer[1];
		pPacket->m_NumChunks = pBuffer[2];
		pPacket->m_DataSize = Size - DataStart;

		if(Sixup)
		{
			unsigned Flags = 0;
			if(pPacket->m_Flags & 1)
				Flags |= NET_PACKETFLAG_CONTROL;
			if(pPacket->m_Flags & 2)
				Flags |= NET_PACKETFLAG_RESEND;
			if(pPacket->m_Flags & 4)
				Flags |= NET_PACKETFLAG_COMPRESSION;
			pPacket->m_Flags = Flags;

			*pSecurityToken = ToSecurityToken(pBuffer + 3);
		}

		if(pPacket->m_Flags & NET_PACKETFLAG_COMPRESSION)
		{
			// Don't allow compressed control packets.
			if(pPacket->m_Flags & NET_PACKETFLAG_CONTROL)
			{
				return -1;
			}
			pPacket->m_DataSize = ms_Huffman.Decompress(&pBuffer[DataStart], pPacket->m_DataSize, pPacket->m_aChunkData, sizeof(pPacket->m_aChunkData));
		}
		else
			mem_copy(pPacket->m_aChunkData, &pBuffer[DataStart], pPacket->m_DataSize);
	}

	// check for errors
	if(pPacket->m_DataSize < 0)
	{
		if(g_Config.m_Debug)
			dbg_msg("network", "error during packet decoding");
		return -1;
	}

	// set the response token (a bit hacky because this function shouldn't know about control packets)
	if(pPacket->m_Flags & NET_PACKETFLAG_CONTROL)
	{
		if(pPacket->m_DataSize >= 5) // control byte + token
		{
			if(pPacket->m_aChunkData[0] == NET_CTRLMSG_CONNECT || pPacket->m_aChunkData[0] == NET_CTRLMSG_TOKEN)
			{
				*pResponseToken = ToSecurityToken(&pPacket->m_aChunkData[1]);
			}
		}
	}

	// log the data
	if(ms_DataLogRecv)
	{
		int Type = 1;
		io_write(ms_DataLogRecv, &Type, sizeof(Type));
		io_write(ms_DataLogRecv, &pPacket->m_DataSize, sizeof(pPacket->m_DataSize));
		io_write(ms_DataLogRecv, pPacket->m_aChunkData, pPacket->m_DataSize);
		io_flush(ms_DataLogRecv);
	}

	// return success
	return 0;
}

void CNetBase::SendControlMsg(NETSOCKET Socket, NETADDR *pAddr, int Ack, int ControlMsg, const void *pExtra, int ExtraSize, SECURITY_TOKEN SecurityToken, bool Sixup)
{
	CNetPacketConstruct Construct;
	Construct.m_Flags = NET_PACKETFLAG_CONTROL;
	Construct.m_Ack = Ack;
	Construct.m_NumChunks = 0;
	Construct.m_DataSize = 1 + ExtraSize;
	Construct.m_aChunkData[0] = ControlMsg;
	if(pExtra)
		mem_copy(&Construct.m_aChunkData[1], pExtra, ExtraSize);

	// send the control message
	CNetBase::SendPacket(Socket, pAddr, &Construct, SecurityToken, Sixup, true);
}

void CNetBase::SendControlMsgWithToken7(NETSOCKET Socket, NETADDR *pAddr, TOKEN Token, int Ack, int ControlMsg, TOKEN MyToken, bool Extended)
{
	dbg_assert((Token & ~NET_TOKEN_MASK) == 0, "token out of range");
	dbg_assert((MyToken & ~NET_TOKEN_MASK) == 0, "resp token out of range");

	unsigned char s_aRequestTokenBuf[NET_TOKENREQUEST_DATASIZE];
	s_aRequestTokenBuf[0] = (MyToken >> 24) & 0xff;
	s_aRequestTokenBuf[1] = (MyToken >> 16) & 0xff;
	s_aRequestTokenBuf[2] = (MyToken >> 8) & 0xff;
	s_aRequestTokenBuf[3] = (MyToken)&0xff;
	CNetBase::SendControlMsg(Socket, pAddr, 0, ControlMsg, s_aRequestTokenBuf, Extended ? sizeof(s_aRequestTokenBuf) : 4, Token, true);
}

unsigned char *CNetChunkHeader::Pack(unsigned char *pData, int Split) const
{
	pData[0] = ((m_Flags & 3) << 6) | ((m_Size >> Split) & 0x3f);
	pData[1] = (m_Size & ((1 << Split) - 1));
	if(m_Flags & NET_CHUNKFLAG_VITAL)
	{
		pData[1] |= (m_Sequence >> 2) & (~((1 << Split) - 1));
		pData[2] = m_Sequence & 0xff;
		return pData + 3;
	}
	return pData + 2;
}

unsigned char *CNetChunkHeader::Unpack(unsigned char *pData, int Split)
{
	m_Flags = (pData[0] >> 6) & 3;
	m_Size = ((pData[0] & 0x3f) << Split) | (pData[1] & ((1 << Split) - 1));
	m_Sequence = -1;
	if(m_Flags & NET_CHUNKFLAG_VITAL)
	{
		m_Sequence = ((pData[1] & (~((1 << Split) - 1))) << 2) | pData[2];
		return pData + 3;
	}
	return pData + 2;
}

bool CNetBase::IsSeqInBackroom(int Seq, int Ack)
{
	int Bottom = (Ack - NET_MAX_SEQUENCE / 2);
	if(Bottom < 0)
	{
		if(Seq <= Ack)
			return true;
		if(Seq >= (Bottom + NET_MAX_SEQUENCE))
			return true;
	}
	else
	{
		if(Seq <= Ack && Seq >= Bottom)
			return true;
	}

	return false;
}

IOHANDLE CNetBase::ms_DataLogSent = nullptr;
IOHANDLE CNetBase::ms_DataLogRecv = nullptr;
CHuffman CNetBase::ms_Huffman;

void CNetBase::OpenLog(IOHANDLE DataLogSent, IOHANDLE DataLogRecv)
{
	if(DataLogSent)
	{
		ms_DataLogSent = DataLogSent;
		dbg_msg("network", "logging sent packages");
	}
	else
		dbg_msg("network", "failed to start logging sent packages");

	if(DataLogRecv)
	{
		ms_DataLogRecv = DataLogRecv;
		dbg_msg("network", "logging recv packages");
	}
	else
		dbg_msg("network", "failed to start logging recv packages");
}

void CNetBase::CloseLog()
{
	if(ms_DataLogSent)
	{
		dbg_msg("network", "stopped logging sent packages");
		io_close(ms_DataLogSent);
		ms_DataLogSent = nullptr;
	}

	if(ms_DataLogRecv)
	{
		dbg_msg("network", "stopped logging recv packages");
		io_close(ms_DataLogRecv);
		ms_DataLogRecv = nullptr;
	}
}

int CNetBase::Compress(const void *pData, int DataSize, void *pOutput, int OutputSize)
{
	return ms_Huffman.Compress(pData, DataSize, pOutput, OutputSize);
}

int CNetBase::Decompress(const void *pData, int DataSize, void *pOutput, int OutputSize)
{
	return ms_Huffman.Decompress(pData, DataSize, pOutput, OutputSize);
}

void CNetBase::Init()
{
	ms_Huffman.Init();
}

void CNetTokenCache::Init(NETSOCKET Socket)
{
	m_Socket = Socket;
}

void CNetTokenCache::SendPacketConnless(CNetChunk *pChunk)
{
	TOKEN Token = GetToken(&pChunk->m_Address);

	if(Token != NET_TOKEN_NONE)
	{
		CNetBase::SendPacketConnlessWithToken7(m_Socket, &pChunk->m_Address, pChunk->m_pData, pChunk->m_DataSize, Token, GenerateToken());
	}
	else
	{
		FetchToken(&pChunk->m_Address);

		CConnlessPacketInfo ConnlessPacket;
		ConnlessPacket.m_Addr = pChunk->m_Address;
		ConnlessPacket.m_Addr.type = pChunk->m_Address.type & ~(NETTYPE_IPV4 | NETTYPE_IPV6);
		mem_copy(ConnlessPacket.m_aData, pChunk->m_pData, pChunk->m_DataSize);
		ConnlessPacket.m_DataSize = pChunk->m_DataSize;
		ConnlessPacket.m_Expiry = time_get() + time_freq() * NET_TOKENCACHE_PACKETEXPIRY;

		unsigned int NetType = pChunk->m_Address.type;
		auto SavePacketFor = [&](unsigned int Type) {
			if(NetType & Type)
			{
				ConnlessPacket.m_Addr.type |= Type;
				m_ConnlessPackets.push_back(ConnlessPacket);
				ConnlessPacket.m_Addr.type &= ~Type;
			}
		};

		SavePacketFor(NETTYPE_IPV4);
		SavePacketFor(NETTYPE_IPV6);
	}
}

void CNetTokenCache::FetchToken(NETADDR *pAddr)
{
	CNetBase::SendControlMsgWithToken7(m_Socket, pAddr, NET_TOKEN_NONE, 0, NET_CTRLMSG_TOKEN, GenerateToken(), true);
}

void CNetTokenCache::AddToken(const NETADDR *pAddr, TOKEN Token)
{
	if(Token == NET_TOKEN_NONE)
		return;

	NETADDR NullAddr = NETADDR_ZEROED;
	NullAddr.port = pAddr->port;
	NullAddr.type = pAddr->type | NETTYPE_LINK_BROADCAST;

	for(auto Iter = m_ConnlessPackets.begin(); Iter != m_ConnlessPackets.end();)
	{
		if(Iter->m_Addr == NullAddr)
		{
			CNetBase::SendPacketConnlessWithToken7(m_Socket, &Iter->m_Addr, Iter->m_aData, Iter->m_DataSize, Token, GenerateToken());

			Iter = m_ConnlessPackets.erase(Iter);
		}
		else
		{
			Iter++;
		}
	}

	CAddressInfo Info;
	Info.m_Addr = *pAddr,
	Info.m_Token = Token,
	Info.m_Expiry = time_get() + (time_freq() * NET_TOKENCACHE_ADDRESSEXPIRY);

	m_TokenCache.push_back(Info);
}

TOKEN CNetTokenCache::GetToken(const NETADDR *pAddr)
{
	for(const auto &AddrInfo : m_TokenCache)
	{
		if(AddrInfo.m_Addr == *pAddr)
		{
			return AddrInfo.m_Token;
		}
	}

	return NET_TOKEN_NONE;
}

TOKEN CNetTokenCache::GenerateToken()
{
	TOKEN Token;
	secure_random_fill(&Token, sizeof(Token));
	return Token;
}

void CNetTokenCache::Update()
{
	int64_t Now = time_get();

	m_TokenCache.erase(
		std::remove_if(m_TokenCache.begin(), m_TokenCache.end(), [&](const CAddressInfo &Info) {
			return Info.m_Expiry <= Now;
		}),
		m_TokenCache.end());

	m_ConnlessPackets.erase(
		std::remove_if(m_ConnlessPackets.begin(), m_ConnlessPackets.end(), [&](CConnlessPacketInfo &Packet) {
			return Packet.m_Expiry <= Now;
		}),
		m_ConnlessPackets.end());
}
