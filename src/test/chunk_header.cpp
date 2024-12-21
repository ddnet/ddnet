#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/network.h>

TEST(ChunkHeader, Seq255)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 255;

	unsigned char aData[8];
	unsigned char *pData = &aData[0];
	Header.Pack(pData);
	EXPECT_EQ(pData[0], 0x40);
	EXPECT_EQ(pData[1], 0x30); // this is weird
	EXPECT_EQ(pData[2], 0xff);
}

TEST(ChunkHeader, Seq5)
{
	CNetChunkHeader Header;
	Header.m_Flags = NET_CHUNKFLAG_VITAL;
	Header.m_Size = 0;
	Header.m_Sequence = 5;

	unsigned char aData[8];
	unsigned char *pData = &aData[0];
	Header.Pack(pData);
	EXPECT_EQ(pData[0], 0x40);
	EXPECT_EQ(pData[1], 0x00);
	EXPECT_EQ(pData[2], 0x05);
}
