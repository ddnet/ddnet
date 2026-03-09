#include <base/mem.h>

#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <gtest/gtest.h>

TEST(Snapshot, CrcOneInt)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 4;
	Flag.m_Y = 0;
	Flag.m_Team = 0;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 4);
}

TEST(Snapshot, CrcTwoInts)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 1;
	Flag.m_Y = 1;
	Flag.m_Team = 0;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 2);
}

TEST(Snapshot, CrcBiggerInts)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 99999999;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 100000001);
}

TEST(Snapshot, CrcOverflow)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 0xFFFFFFFF;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 1);
}
