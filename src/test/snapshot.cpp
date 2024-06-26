#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/snapshot.h>
#include <game/generated/protocol.h>

TEST(Snapshot, CrcOneInt)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	void *pItem = Builder.NewItem(CNetObj_Flag::ms_MsgId, 0, sizeof(Flag));
	ASSERT_FALSE(pItem == nullptr);
	Flag.m_X = 4;
	Flag.m_Y = 0;
	Flag.m_Team = 0;
	mem_copy(pItem, &Flag, sizeof(Flag));

	char aData[CSnapshot::MAX_SIZE];
	CSnapshot *pSnapshot = (CSnapshot *)aData;
	Builder.Finish(pSnapshot);

	ASSERT_EQ(pSnapshot->Crc(), 4);
}

TEST(Snapshot, CrcTwoInts)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	void *pItem = Builder.NewItem(CNetObj_Flag::ms_MsgId, 0, sizeof(Flag));
	ASSERT_FALSE(pItem == nullptr);
	Flag.m_X = 1;
	Flag.m_Y = 1;
	Flag.m_Team = 0;
	mem_copy(pItem, &Flag, sizeof(Flag));

	char aData[CSnapshot::MAX_SIZE];
	CSnapshot *pSnapshot = (CSnapshot *)aData;
	Builder.Finish(pSnapshot);

	ASSERT_EQ(pSnapshot->Crc(), 2);
}

TEST(Snapshot, CrcBiggerInts)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	void *pItem = Builder.NewItem(CNetObj_Flag::ms_MsgId, 0, sizeof(Flag));
	ASSERT_FALSE(pItem == nullptr);
	Flag.m_X = 99999999;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	mem_copy(pItem, &Flag, sizeof(Flag));

	char aData[CSnapshot::MAX_SIZE];
	CSnapshot *pSnapshot = (CSnapshot *)aData;
	Builder.Finish(pSnapshot);

	ASSERT_EQ(pSnapshot->Crc(), 100000001);
}

TEST(Snapshot, CrcOverflow)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	void *pItem = Builder.NewItem(CNetObj_Flag::ms_MsgId, 0, sizeof(Flag));
	ASSERT_FALSE(pItem == nullptr);
	Flag.m_X = 0xFFFFFFFF;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	mem_copy(pItem, &Flag, sizeof(Flag));

	char aData[CSnapshot::MAX_SIZE];
	CSnapshot *pSnapshot = (CSnapshot *)aData;
	Builder.Finish(pSnapshot);

	ASSERT_EQ(pSnapshot->Crc(), 1);
}
