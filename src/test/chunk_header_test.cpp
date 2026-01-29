#include "test.h"

#include <base/system.h>

#include <engine/shared/network.h>

#include <gtest/gtest.h>

static void TestPackUnpack(
	const CNetChunkHeader &Header,
	int Split,
	std::initializer_list<unsigned char> ExpectedPacked)
{
	unsigned char aData[3];
	ASSERT_LE(ExpectedPacked.size(), std::size(aData));
	unsigned char *pData = &aData[0];
	const int BytesPacked = Header.Pack(pData, Split) - pData;
	ASSERT_EQ(BytesPacked, ExpectedPacked.size());

	// Unpack function expects non-const pointer but does not modify it
	unsigned char *pPacked = const_cast<unsigned char *>(std::data(ExpectedPacked));
	for(size_t i = 0; i < ExpectedPacked.size(); ++i)
	{
		EXPECT_EQ(pData[i], pPacked[i]);
	}

	CNetChunkHeader UnpackedHeader{};
	const int BytesUnpacked = UnpackedHeader.Unpack(pPacked, Split) - pPacked;
	ASSERT_EQ(BytesUnpacked, ExpectedPacked.size());
	EXPECT_EQ(Header.m_Flags, UnpackedHeader.m_Flags);
	EXPECT_EQ(Header.m_Size, UnpackedHeader.m_Size);
	if(BytesUnpacked == 3)
	{
		EXPECT_EQ(UnpackedHeader.m_Sequence, Header.m_Sequence);
	}
	else
	{
		EXPECT_EQ(UnpackedHeader.m_Sequence, -1);
	}
}

TEST(ChunkHeader, PackZeroed)
{
	CNetChunkHeader Header;
	Header.m_Flags = 0;
	Header.m_Size = 0;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x00, 0x00});
	TestPackUnpack(Header, 6, {0x00, 0x00});
}

TEST(ChunkHeader, PackFlagVital)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x40, 0x00, 0x00});
	TestPackUnpack(Header, 6, {0x40, 0x00, 0x00});
}

TEST(ChunkHeader, PackFlagResend)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_RESEND;
	Header.m_Size = 0;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x80, 0x00});
	TestPackUnpack(Header, 6, {0x80, 0x00});
}

TEST(ChunkHeader, PackFlagVitalResend)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL | NET_CHUNKFLAG_RESEND;
	Header.m_Size = 0;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0xC0, 0x00, 0x00});
	TestPackUnpack(Header, 6, {0xC0, 0x00, 0x00});
}

TEST(ChunkHeader, PackSize15)
{
	CNetChunkHeader Header;
	Header.m_Flags = 0;
	Header.m_Size = 15;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x00, 0x0F});
	TestPackUnpack(Header, 6, {0x00, 0x0F});
}

TEST(ChunkHeader, PackSize255)
{
	CNetChunkHeader Header;
	Header.m_Flags = 0;
	Header.m_Size = 255;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x0F, 0x0F});
	TestPackUnpack(Header, 6, {0x03, 0x3F});
}

TEST(ChunkHeader, PackSize511)
{
	CNetChunkHeader Header;
	Header.m_Flags = 0;
	Header.m_Size = 511;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x1F, 0x0F});
	TestPackUnpack(Header, 6, {0x07, 0x3F});
}

TEST(ChunkHeader, PackSize1023)
{
	CNetChunkHeader Header;
	Header.m_Flags = 0;
	Header.m_Size = 1023;
	Header.m_Sequence = 0;
	TestPackUnpack(Header, 4, {0x3F, 0x0F});
	TestPackUnpack(Header, 6, {0x0F, 0x3F});
}

TEST(ChunkHeader, PackSize4095)
{
	CNetChunkHeader Header;
	Header.m_Flags = 0;
	Header.m_Size = 4095;
	Header.m_Sequence = 0;
	// The maximum size with Split=6 is 4095, whereas with Split=4 it is 1023
	TestPackUnpack(Header, 6, {0x3F, 0x3F});
}

TEST(ChunkHeader, PackSeq5)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 5;
	TestPackUnpack(Header, 4, {0x40, 0x00, 0x05});
	TestPackUnpack(Header, 6, {0x40, 0x00, 0x05});
}

TEST(ChunkHeader, PackSeq63)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 63;
	TestPackUnpack(Header, 4, {0x40, 0x00, 0x3F});
	TestPackUnpack(Header, 6, {0x40, 0x00, 0x3F});
}

TEST(ChunkHeader, PackSeq64)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 64;
	TestPackUnpack(Header, 4, {0x40, 0x00, 0x40});
	TestPackUnpack(Header, 6, {0x40, 0x00, 0x40});
}

TEST(ChunkHeader, PackSeq126)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 126;
	TestPackUnpack(Header, 4, {0x40, 0x00, 0x7E});
	TestPackUnpack(Header, 6, {0x40, 0x00, 0x7E});
}

TEST(ChunkHeader, PackSeq255)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 255;
	TestPackUnpack(Header, 4, {0x40, 0x00, 0xFF});
	TestPackUnpack(Header, 6, {0x40, 0x00, 0xFF});
}

TEST(ChunkHeader, PackSeqMax)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = NET_MAX_SEQUENCE - 1;
	TestPackUnpack(Header, 4, {0x40, 0xC0, 0xFF});
	TestPackUnpack(Header, 6, {0x40, 0xC0, 0xFF});
}

TEST(ChunkHeader, PackSize255Seq511)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 255;
	Header.m_Sequence = 511;
	TestPackUnpack(Header, 4, {0x4F, 0x4F, 0xFF});
	TestPackUnpack(Header, 6, {0x43, 0x7F, 0xFF});
}

TEST(ChunkHeader, PackAllBits)
{
	CNetChunkHeader HeaderSplit4;
	HeaderSplit4.m_Flags = NET_CHUNKFLAG_VITAL | NET_CHUNKFLAG_RESEND;
	HeaderSplit4.m_Size = 1023;
	HeaderSplit4.m_Sequence = NET_MAX_SEQUENCE - 1;

	CNetChunkHeader HeaderSplit6;
	HeaderSplit6.m_Flags = NET_CHUNKFLAG_VITAL | NET_CHUNKFLAG_RESEND;
	HeaderSplit6.m_Size = 4095;
	HeaderSplit6.m_Sequence = NET_MAX_SEQUENCE - 1;

	// Packing with Split=4 leaves 2 bits unused
	TestPackUnpack(HeaderSplit4, 4, {0xFF, 0xCF, 0xFF});
	TestPackUnpack(HeaderSplit6, 6, {0xFF, 0xFF, 0xFF});
}
