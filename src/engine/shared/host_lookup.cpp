/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "host_lookup.h"

#include <base/system.h>

#if defined(CONF_FAMILY_WINDOWS)
#include <winsock2.h>
#endif

CHostLookup::CHostLookup() = default;

CHostLookup::CHostLookup(const char *pHostname, int Nettype)
{
	str_copy(m_aHostname, pHostname);
	m_Nettype = Nettype;
	m_DnsServer = NETADDR_ZEROED;
	Abortable(true);
}

CHostLookup::CHostLookup(const char *pHostname, int Nettype, const NETADDR &DnsServer)
{
	str_copy(m_aHostname, pHostname);
	m_Nettype = Nettype;
	m_DnsServer = DnsServer;
	Abortable(true);
}

struct SDnsHeader
{
	uint16_t m_Id;
	uint16_t m_Flags;
	uint16_t m_Qdcount;
	uint16_t m_Ancount;
	uint16_t m_Nscount;
	uint16_t m_Arcount;
};

static int BuildDnsQuery(unsigned char *pBuffer, int BufferSize, const char *pHostname, uint16_t QType)
{
	if(BufferSize < 512)
		return -1;

	unsigned char *pCur = pBuffer;

	// DNS Header
	SDnsHeader *pHeader = (SDnsHeader *)pCur;
	pHeader->m_Id = htons(0x1234); // Transaction ID
	pHeader->m_Flags = htons(0x0100); // Standard recursive query
	pHeader->m_Qdcount = htons(1); // One question
	pHeader->m_Ancount = 0;
	pHeader->m_Nscount = 0;
	pHeader->m_Arcount = 0;
	pCur += sizeof(SDnsHeader);

	// Question section - encode hostname
	const char *pLabel = pHostname;
	while(*pLabel)
	{
		const char *pDot = str_find(pLabel, ".");
		int LabelLen = pDot ? (pDot - pLabel) : str_length(pLabel);

		if(LabelLen > 63 || pCur - pBuffer + LabelLen + 1 >= BufferSize - 4)
			return -1;

		*pCur = (unsigned char)LabelLen;
		pCur++;
		mem_copy(pCur, pLabel, LabelLen);
		pCur += LabelLen;

		if(!pDot)
			break;
		pLabel = pDot + 1;
	}
	*pCur = 0; // End of hostname
	pCur++;

	// Question type and class
	*(uint16_t *)pCur = htons(QType); // QTYPE (A=1 or AAAA=28)
	pCur += 2;
	*(uint16_t *)pCur = htons(1); // QCLASS (IN=1)
	pCur += 2;

	return pCur - pBuffer;
}

static int ParseDnsResponse(const unsigned char *pResponse, int ResponseSize, NETADDR *pAddr, int Nettype)
{
	if(ResponseSize < (int)sizeof(SDnsHeader))
		return -1;

	const unsigned char *pCur = pResponse;
	const SDnsHeader *pHeader = (const SDnsHeader *)pCur;
	pCur += sizeof(SDnsHeader);

	uint16_t Flags = ntohs(pHeader->m_Flags);
	uint16_t Rcode = Flags & 0x000F;

	// Check for NXDOMAIN (error code 3)
	if(Rcode == 3)
		return 1; // NXDOMAIN

	if(Rcode != 0)
		return -1; // Other error

	uint16_t Qdcount = ntohs(pHeader->m_Qdcount);
	uint16_t Ancount = ntohs(pHeader->m_Ancount);

	// Skip question section
	for(uint16_t Question = 0; Question < Qdcount; Question++)
	{
		// Skip name
		while(pCur < pResponse + ResponseSize)
		{
			if(*pCur == 0) // End of name
			{
				pCur++;
				break;
			}
			if((*pCur & 0xC0) == 0xC0) // Compressed name
			{
				pCur += 2;
				break;
			}
			pCur += *pCur + 1;
		}
		if(pCur + 4 > pResponse + ResponseSize)
			return -1;
		pCur += 4; // Skip QTYPE and QCLASS
	}

	// Parse answer section
	for(uint16_t Answer = 0; Answer < Ancount; Answer++)
	{
		// Skip name
		while(pCur < pResponse + ResponseSize)
		{
			if(*pCur == 0) // End of name
			{
				pCur++;
				break;
			}
			if((*pCur & 0xC0) == 0xC0) // Compressed name
			{
				pCur += 2;
				break;
			}
			pCur += *pCur + 1;
		}

		if(pCur + 10 > pResponse + ResponseSize)
			return -1;

		uint16_t Type = ntohs(*(uint16_t *)pCur);
		pCur += 2;
		pCur += 2; // Skip class
		pCur += 4; // Skip TTL
		uint16_t RdLength = ntohs(*(uint16_t *)pCur);
		pCur += 2;

		if(pCur + RdLength > pResponse + ResponseSize)
			return -1;

		// Check if this is the record we want
		if((Nettype & NETTYPE_IPV4) && Type == 1 && RdLength == 4) // A record
		{
			mem_zero(pAddr, sizeof(NETADDR));
			pAddr->type = NETTYPE_IPV4;
			mem_copy(pAddr->ip, pCur, 4);
			return 0; // Success
		}
		else if((Nettype & NETTYPE_IPV6) && Type == 28 && RdLength == 16) // AAAA record
		{
			mem_zero(pAddr, sizeof(NETADDR));
			pAddr->type = NETTYPE_IPV6;
			mem_copy(pAddr->ip, pCur, 16);
			return 0; // Success
		}

		pCur += RdLength;
	}

	return -1; // No matching record found
}

void CHostLookup::Run()
{
	// Check if we should use custom DNS server
	if(m_DnsServer.type == NETTYPE_INVALID)
	{
		// Fall back to system resolver
		m_Result = net_host_lookup(m_aHostname, &m_Addr, m_Nettype);
		return;
	}

	// Create UDP socket
	NETADDR BindAddr = NETADDR_ZEROED;
	BindAddr.type = m_DnsServer.type;
	NETSOCKET Sock = net_udp_create(BindAddr);
	if(net_socket_type(Sock) == NETTYPE_INVALID)
	{
		m_Result = -1;
		return;
	}

	// Build DNS query
	unsigned char aQueryBuffer[512];
	uint16_t QType = (m_Nettype & NETTYPE_IPV4) ? 1 : 28; // A or AAAA
	int QuerySize = BuildDnsQuery(aQueryBuffer, sizeof(aQueryBuffer), m_aHostname, QType);
	if(QuerySize < 0)
	{
		net_udp_close(Sock);
		m_Result = -1;
		return;
	}

	// Set DNS server port if not set
	NETADDR DnsServer = m_DnsServer;
	if(DnsServer.port == 0)
		DnsServer.port = 53;

	// Send query
	if(net_udp_send(Sock, &DnsServer, aQueryBuffer, QuerySize) < 0)
	{
		net_udp_close(Sock);
		m_Result = -1;
		return;
	}

	// Wait for response with timeout
	if(net_socket_read_wait(Sock, std::chrono::seconds(5)) <= 0)
	{
		net_udp_close(Sock);
		m_Result = -1;
		return;
	}

	// Receive response
	NETADDR ResponseAddr;
	unsigned char *pResponseData = nullptr;
	int ResponseSize = net_udp_recv(Sock, &ResponseAddr, &pResponseData);

	if(ResponseSize > 0 && pResponseData != nullptr)
	{
		m_Result = ParseDnsResponse(pResponseData, ResponseSize, &m_Addr, m_Nettype);
	}
	else
	{
		m_Result = -1;
	}

	net_udp_close(Sock);
}
