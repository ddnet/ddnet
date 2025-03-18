#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/network.h>

template<size_t PackedSize>
static void AssertHeader(const CNetChunkHeader &Header, unsigned char aPacked[PackedSize])
{
	unsigned char aData[8];
	unsigned char *pData = &aData[0];
	const int BytesWritten = Header.Pack(pData) - pData;
	EXPECT_EQ(BytesWritten, PackedSize);
	EXPECT_EQ(pData[0], aPacked[0]);
	EXPECT_EQ(pData[1], aPacked[1]);
	if(PackedSize > 2)
	{
		EXPECT_EQ(pData[2], aPacked[2]);
	}
}

TEST(ChunkHeader, Seq255)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 255;

	unsigned char aExpected[3] = {
		0x40,
		0x30, // this is weird
		0xff};
	AssertHeader<3>(Header, aExpected);
}

TEST(ChunkHeader, Seq126)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 126;

	unsigned char aExpected[3] = {0x40, 0x10, 0x7e};
	AssertHeader<3>(Header, aExpected);
}

TEST(ChunkHeader, Seq63)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 63;

	unsigned char aExpected[3] = {0x40, 0x00, 0x3f};
	AssertHeader<3>(Header, aExpected);
}

TEST(ChunkHeader, Seq64)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 64;

	unsigned char aExpected[3] = {0x40, 0x10, 0x40};
	AssertHeader<3>(Header, aExpected);
}

TEST(ChunkHeader, Seq5)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 5;

	unsigned char aExpected[3] = {0x40, 0x00, 0x05};
	AssertHeader<3>(Header, aExpected);
}
