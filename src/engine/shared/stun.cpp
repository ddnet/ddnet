#include "stun.h"

#include <base/system.h>

// STUN header (from RFC 5389, section 6, figure 2):
//
//        0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |0 0|     STUN Message Type     |         Message Length        |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                         Magic Cookie                          |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |                                                               |
//      |                     Transaction ID (96 bits)                  |
//      |                                                               |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

size_t StunMessagePrepare(unsigned char *pBuffer, size_t BufferSize, CStunData *pData)
{
	dbg_assert(BufferSize >= 20, "stun message buffer too small");
	secure_random_fill(pData->m_aSecret, sizeof(pData->m_aSecret));
	pBuffer[0] = 0x00; // STUN Request Message Type 1: Binding
	pBuffer[1] = 0x01;
	pBuffer[2] = 0x00; // Message Length: 0 (extra bytes after the header)
	pBuffer[3] = 0x00;
	pBuffer[4] = 0x21; // Magic Cookie: 0x2112A442
	pBuffer[5] = 0x12;
	pBuffer[6] = 0xA4;
	pBuffer[7] = 0x42;
	mem_copy(pBuffer + 8, pData->m_aSecret, sizeof(pData->m_aSecret)); // Transaction ID
	return 20;
}

bool StunMessageParse(const unsigned char *pMessage, size_t MessageSize, const CStunData *pData, bool *pSuccess, NETADDR *pAddr)
{
	*pSuccess = false;
	mem_zero(pAddr, sizeof(*pAddr));
	if(MessageSize < 20)
	{
		return true;
	}
	bool Parsed = true;
	// STUN Success/Error Response Type 1: Binding
	Parsed = Parsed && pMessage[0] == 0x01 && (pMessage[1] == 0x01 || pMessage[1] == 0x11);
	uint16_t MessageLength = (pMessage[2] << 8) | pMessage[3];
	Parsed = Parsed && MessageSize >= 20 + (size_t)MessageLength && MessageLength % 4 == 0;
	// Magic Cookie: 0x2112A442
	Parsed = Parsed && pMessage[4] == 0x21 && pMessage[5] == 0x12;
	Parsed = Parsed && pMessage[6] == 0xA4 && pMessage[7] == 0x42;
	// Transaction ID
	Parsed = Parsed && mem_comp(pMessage + 8, pData->m_aSecret, sizeof(pData->m_aSecret)) == 0;
	if(!Parsed)
	{
		return true;
	}

	*pSuccess = pMessage[1] == 0x01;

	MessageSize = 20 + MessageLength;
	size_t Offset = 20;
	bool FoundAddr = false;
	while(true)
	{
		// STUN attribute format (from RFC 5389, section 15, figure 4):
		//
		//        0                   1                   2                   3
		//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//      |         Type                  |            Length             |
		//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//      |                         Value (variable)                ....
		//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		if(MessageSize == Offset)
		{
			break;
		}
		else if(MessageSize < Offset + 4)
		{
			return true;
		}
		uint16_t Type = (pMessage[Offset] << 8) | pMessage[Offset + 1];
		uint16_t Length = (pMessage[Offset + 2] << 8) | pMessage[Offset + 3];
		if(MessageSize < Offset + 4 + Length)
		{
			return true;
		}
		if(*pSuccess && Type == 0x0020) // XOR-MAPPED-ADDRESS
		{
			// STUN XOR-MAPPED-ADDRESS attribute format (from RFC 5389, section 15,
			// figure 6):
			//
			//       0                   1                   2                   3
			//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//     |x x x x x x x x|    Family     |         X-Port                |
			//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//     |                X-Address (Variable)
			//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

			if(Length < 4)
			{
				return true;
			}
			// Only use the first found address.
			uint8_t Family = pMessage[Offset + 4 + 1];
			uint16_t Port = (pMessage[Offset + 4 + 2] << 8) | pMessage[Offset + 4 + 3];
			Port ^= 0x2112;
			if(Family == 0x01) // IPv4
			{
				if(Length != 8)
				{
					return true;
				}
				if(!FoundAddr)
				{
					pAddr->type = NETTYPE_IPV4;
					mem_copy(pAddr->ip, pMessage + Offset + 4 + 4, 4);
					pAddr->ip[0] ^= 0x21;
					pAddr->ip[1] ^= 0x12;
					pAddr->ip[2] ^= 0xA4;
					pAddr->ip[3] ^= 0x42;
					pAddr->port = Port;
					FoundAddr = true;
				}
			}
			else if(Family == 0x02) // IPv6
			{
				if(Length != 20)
				{
					return true;
				}
				if(!FoundAddr)
				{
					pAddr->type = NETTYPE_IPV6;
					mem_copy(pAddr->ip, pMessage + Offset + 4 + 4, 16);
					pAddr->ip[0] ^= 0x21;
					pAddr->ip[1] ^= 0x12;
					pAddr->ip[2] ^= 0xA4;
					pAddr->ip[3] ^= 0x42;
					for(size_t i = 0; i < sizeof(pData->m_aSecret); i++)
					{
						pAddr->ip[4 + i] ^= pData->m_aSecret[i];
					}
					pAddr->port = Port;
					FoundAddr = true;
				}
			}
		}
		// comprehension-required
		else if(Type <= 0x7fff)
		{
			return true;
		}
		Offset += 4 + Length;
	}
	return *pSuccess && !FoundAddr;
}
