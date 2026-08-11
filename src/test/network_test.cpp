#include <engine/shared/network.h>

#include <gtest/gtest.h>

static int UnpackUncompressedPacket(int Size, CNetPacketConstruct *pPacket)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = 0; // no flags, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, Size, pPacket, Sixup);
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
