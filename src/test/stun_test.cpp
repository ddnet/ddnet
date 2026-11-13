#include <base/mem.h>
#include <base/net.h>

#include <engine/shared/stun.h>

#include <gtest/gtest.h>

static const unsigned char s_aMagicCookie[4] = {0x21, 0x12, 0xA4, 0x42};

// Builds a STUN binding success response containing a single
// XOR-MAPPED-ADDRESS attribute for the given address.
static size_t BuildResponse(unsigned char *pBuffer, const CStunData &StunData, int Family, const unsigned char *pIp, int Port)
{
	const size_t IpSize = Family == NETTYPE_IPV4 ? 4 : 16;
	const size_t AttrValueSize = 4 + IpSize;
	const size_t MessageLength = 4 + AttrValueSize; // attribute header and value
	mem_zero(pBuffer, 20 + MessageLength);

	// STUN header
	pBuffer[0] = 0x01; // binding success response
	pBuffer[1] = 0x01;
	pBuffer[2] = (MessageLength >> 8) & 0xff;
	pBuffer[3] = MessageLength & 0xff;
	mem_copy(pBuffer + 4, s_aMagicCookie, sizeof(s_aMagicCookie));
	mem_copy(pBuffer + 8, StunData.m_aSecret, sizeof(StunData.m_aSecret));

	// XOR-MAPPED-ADDRESS attribute
	unsigned char *pAttr = pBuffer + 20;
	pAttr[0] = 0x00;
	pAttr[1] = 0x20;
	pAttr[2] = (AttrValueSize >> 8) & 0xff;
	pAttr[3] = AttrValueSize & 0xff;
	pAttr[4] = 0x00;
	pAttr[5] = Family == NETTYPE_IPV4 ? 0x01 : 0x02;
	const int XPort = Port ^ 0x2112;
	pAttr[6] = (XPort >> 8) & 0xff;
	pAttr[7] = XPort & 0xff;
	mem_copy(pAttr + 8, pIp, IpSize);
	pAttr[8] ^= 0x21;
	pAttr[9] ^= 0x12;
	pAttr[10] ^= 0xA4;
	pAttr[11] ^= 0x42;
	for(size_t i = 4; i < IpSize; i++)
	{
		pAttr[8 + i] ^= StunData.m_aSecret[i - 4];
	}

	return 20 + MessageLength;
}

TEST(Stun, ParseIpv4Response)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	const unsigned char aIp[4] = {93, 184, 216, 34};
	unsigned char aResponse[64];
	const size_t ResponseSize = BuildResponse(aResponse, Data, NETTYPE_IPV4, aIp, 54321);

	NETADDR Addr;
	bool Success = false;
	EXPECT_FALSE(StunMessageParse(aResponse, ResponseSize, &Data, &Success, &Addr));
	EXPECT_TRUE(Success);
	EXPECT_EQ(Addr.type, NETTYPE_IPV4);
	EXPECT_EQ(Addr.port, 54321);
	EXPECT_EQ(mem_comp(Addr.ip, aIp, sizeof(aIp)), 0);
}

TEST(Stun, ParseIpv6Response)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	const unsigned char aIp[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
	unsigned char aResponse[64];
	const size_t ResponseSize = BuildResponse(aResponse, Data, NETTYPE_IPV6, aIp, 54321);

	NETADDR Addr;
	bool Success = false;
	EXPECT_FALSE(StunMessageParse(aResponse, ResponseSize, &Data, &Success, &Addr));
	EXPECT_TRUE(Success);
	EXPECT_EQ(Addr.type, NETTYPE_IPV6);
	EXPECT_EQ(Addr.port, 54321);
	EXPECT_EQ(mem_comp(Addr.ip, aIp, sizeof(aIp)), 0);
}

TEST(Stun, ParseResponseWrongTransactionId)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	const unsigned char aIp[4] = {127, 0, 0, 1};
	unsigned char aResponse[64];
	const size_t ResponseSize = BuildResponse(aResponse, Data, NETTYPE_IPV4, aIp, 1234);
	aResponse[8] ^= 0x01; // transaction ID does not match the request

	NETADDR Addr;
	bool Success = false;
	EXPECT_TRUE(StunMessageParse(aResponse, ResponseSize, &Data, &Success, &Addr));
	EXPECT_FALSE(Success);
}

TEST(Stun, ParseNonStunPacket)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	unsigned char aPacket[20] = {};
	NETADDR Addr;
	bool Success = false;
	EXPECT_TRUE(StunMessageParse(aPacket, sizeof(aPacket), &Data, &Success, &Addr));
	EXPECT_FALSE(Success);
}

TEST(Stun, ParseTooShortPacket)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	unsigned char aPacket[19] = {};
	NETADDR Addr;
	bool Success = false;
	EXPECT_TRUE(StunMessageParse(aPacket, sizeof(aPacket), &Data, &Success, &Addr));
	EXPECT_FALSE(Success);
}

TEST(Stun, ParseErrorResponse)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	unsigned char aResponse[20] = {};
	aResponse[0] = 0x01;
	aResponse[1] = 0x11; // binding error response, no attributes
	mem_copy(aResponse + 4, s_aMagicCookie, sizeof(s_aMagicCookie));
	mem_copy(aResponse + 8, Data.m_aSecret, sizeof(Data.m_aSecret));

	NETADDR Addr;
	bool Success = true;
	EXPECT_FALSE(StunMessageParse(aResponse, sizeof(aResponse), &Data, &Success, &Addr));
	EXPECT_FALSE(Success);
}

TEST(Stun, ParseUnknownRequiredAttribute)
{
	CStunData Data;
	unsigned char aRequest[32];
	StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	unsigned char aResponse[24] = {};
	aResponse[0] = 0x01;
	aResponse[1] = 0x01;
	aResponse[2] = 0x00;
	aResponse[3] = 0x04; // one attribute of length 0
	mem_copy(aResponse + 4, s_aMagicCookie, sizeof(s_aMagicCookie));
	mem_copy(aResponse + 8, Data.m_aSecret, sizeof(Data.m_aSecret));
	aResponse[20] = 0x00;
	aResponse[21] = 0x0a; // unknown comprehension-required attribute

	NETADDR Addr;
	bool Success = false;
	// The response is not understood because of the unknown attribute, so its
	// success flag must not be relied upon.
	EXPECT_TRUE(StunMessageParse(aResponse, sizeof(aResponse), &Data, &Success, &Addr));
}

TEST(Stun, PrepareRequest)
{
	CStunData Data;
	unsigned char aRequest[32];
	const size_t RequestSize = StunMessagePrepare(aRequest, sizeof(aRequest), &Data);

	EXPECT_EQ(RequestSize, 20u);
	EXPECT_EQ(aRequest[0], 0x00);
	EXPECT_EQ(aRequest[1], 0x01); // binding request
	EXPECT_EQ(aRequest[2], 0x00); // no attributes
	EXPECT_EQ(aRequest[3], 0x00);
	EXPECT_EQ(mem_comp(aRequest + 4, s_aMagicCookie, sizeof(s_aMagicCookie)), 0);
	EXPECT_EQ(mem_comp(aRequest + 8, Data.m_aSecret, sizeof(Data.m_aSecret)), 0);
}
