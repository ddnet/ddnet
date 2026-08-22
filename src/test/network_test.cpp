#include <engine/shared/network.h>

#include <gtest/gtest.h>

static int UnpackUncompressedPacket(int Size, CNetPacketConstruct *pPacket, bool AllowDecompression = true)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = 0; // no flags, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, Size, pPacket, Sixup, AllowDecompression);
}

static int UnpackCompressedPacket(CNetPacketConstruct *pPacket, bool AllowDecompression, bool *pDecompressed = nullptr, int PayloadSize = 64)
{
	CNetBase::Init();

	const unsigned char aPayload[4 * NET_MAX_PACKETSIZE] = {};
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = (NET_PACKETFLAG_COMPRESSION << 2) & 0xfc; // compression, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	const int CompressedSize = CNetBase::Compress(aPayload, PayloadSize,
		&aBuffer[NET_PACKETHEADERSIZE], NET_MAX_PACKETSIZE - NET_PACKETHEADERSIZE);
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, NET_PACKETHEADERSIZE + CompressedSize, pPacket, Sixup, AllowDecompression, nullptr, nullptr, pDecompressed);
}

TEST(Network, UnpackMaximumUncompressedPacket)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_PACKETHEADERSIZE + (int)sizeof(Packet.m_aChunkData), &Packet), 0);
	EXPECT_EQ(Packet.m_DataSize, (int)sizeof(Packet.m_aChunkData));
	EXPECT_EQ(UnpackUncompressedPacket(NET_MAX_PACKETSIZE, &Packet), 0);
}

TEST(Network, UnpackOversizedUncompressedPacket)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_PACKETHEADERSIZE + (int)sizeof(Packet.m_aChunkData) + 1, &Packet), -1);
}

TEST(Network, UnpackCompressedPacket)
{
	CNetPacketConstruct Packet;
	bool Decompressed = false;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, true, &Decompressed), 0);
	EXPECT_EQ(Packet.m_DataSize, 64);
	EXPECT_TRUE(Decompressed);
}

TEST(Network, UnpackCompressedPacketWithoutDecompression)
{
	CNetPacketConstruct Packet;
	bool Decompressed = true;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, false, &Decompressed), -1);
	EXPECT_FALSE(Decompressed);
}

TEST(Network, UnpackOversizedCompressedPacket)
{
	// Decoding data that does not fit the packet buffer costs the same, so it counts
	// as a decompression all the same.
	CNetPacketConstruct Packet;
	bool Decompressed = false;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, true, &Decompressed, (int)sizeof(Packet.m_aChunkData) + 1), -1);
	EXPECT_TRUE(Decompressed);
}

TEST(Network, UnpackUncompressedPacketWithoutDecompression)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_MAX_PACKETSIZE, &Packet, false), 0);
}

TEST(Network, UnpackChunks)
{
	// Three non-vital chunks of different sizes, each filled with its own byte.
	constexpr int NumChunks = 3;
	const int aSizes[NumChunks] = {1, 100, 7};

	CNetPacketConstruct Packet = {};
	Packet.m_NumChunks = NumChunks;
	unsigned char *pData = Packet.m_aChunkData;
	for(int i = 0; i < NumChunks; i++)
	{
		CNetChunkHeader Header;
		Header.m_Flags = 0;
		Header.m_Size = aSizes[i];
		Header.m_Sequence = 0;
		pData = Header.Pack(pData);
		memset(pData, i, aSizes[i]);
		pData += aSizes[i];
	}
	Packet.m_DataSize = (int)(pData - Packet.m_aChunkData);

	CNetConnection Connection;
	Connection.m_Sixup = false;

	CPacketChunkUnpacker Unpacker;
	Unpacker.FeedPacket(NETADDR{}, Packet, &Connection, 0);
	for(int i = 0; i < NumChunks; i++)
	{
		CNetChunk Chunk;
		ASSERT_TRUE(Unpacker.UnpackNextChunk(&Chunk));
		EXPECT_EQ(Chunk.m_DataSize, aSizes[i]);
		for(int j = 0; j < Chunk.m_DataSize; j++)
		{
			EXPECT_EQ(((const unsigned char *)Chunk.m_pData)[j], i);
		}
	}
	CNetChunk Chunk;
	EXPECT_FALSE(Unpacker.UnpackNextChunk(&Chunk));
}
