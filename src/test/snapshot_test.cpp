#include <base/mem.h>

#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <gtest/gtest.h>

#include <memory>

TEST(Snapshot, CrcOneInt)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 4;
	Flag.m_Y = 0;
	Flag.m_Team = 0;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 4);
}

TEST(Snapshot, CrcTwoInts)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 1;
	Flag.m_Y = 1;
	Flag.m_Team = 0;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 2);
}

TEST(Snapshot, CrcBiggerInts)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 99999999;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 100000001);
}

TEST(Snapshot, CrcOverflow)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 0xFFFFFFFF;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 1);
}

TEST(Snapshot, StorageGet)
{
	CSnapshotStorage Storage;

	// `CSnapshotStorage` needs snapshots in increasing tick order.
	const char aData[8] = {0};
	Storage.Add(10, 1000, 1, aData, 0, nullptr);
	Storage.Add(20, 2000, 2, aData, 0, nullptr);
	Storage.Add(30, 3000, 3, aData, 0, nullptr);
	Storage.Add(40, 4000, 4, aData, 0, nullptr);

	int64_t Tagtime = -1;

	// Retrieve existing snapshots.
	EXPECT_EQ(Storage.Get(40, &Tagtime, nullptr, nullptr), 4);
	EXPECT_EQ(Tagtime, 4000);
	EXPECT_EQ(Storage.Get(10, &Tagtime, nullptr, nullptr), 1);
	EXPECT_EQ(Tagtime, 1000);
	EXPECT_EQ(Storage.Get(30, &Tagtime, nullptr, nullptr), 3);
	EXPECT_EQ(Tagtime, 3000);

	// Check non-existing snapshots in before, within and after the range.
	EXPECT_EQ(Storage.Get(50, nullptr, nullptr, nullptr), -1);
	EXPECT_EQ(Storage.Get(5, nullptr, nullptr, nullptr), -1);
	EXPECT_EQ(Storage.Get(25, nullptr, nullptr, nullptr), -1);
}

// A delta needs more space than the snapshot it was created from: each item of
// the base snapshot that is gone costs a deletion key, and each item whose size
// is not statically known costs an additional size int.
TEST(Snapshot, DeltaWorstCase)
{
	// The buffers are allocated on the heap, together they are larger than the
	// 1 MiB stack that Windows gives the main thread.
	const auto pItemData = std::make_unique<char[]>(CSnapshot::MAX_SIZE);

	// Base snapshot with as many items as possible. None of them is in the
	// other snapshot, so each of them costs a deletion key in the delta.
	const auto pFromBuilder = std::make_unique<CSnapshotBuilder>();
	pFromBuilder->Init();
	for(int Id = 0; Id < CSnapshot::MAX_ITEMS; Id++)
		ASSERT_TRUE(pFromBuilder->NewItem(NETOBJTYPE_PICKUP, Id, pItemData.get(), sizeof(int)));
	const auto pFromBuffer = std::make_unique<CSnapshotBuffer>();
	pFromBuilder->Finish(pFromBuffer.get());
	ASSERT_EQ(pFromBuffer->AsSnapshot()->NumItems(), CSnapshot::MAX_ITEMS);

	// Snapshot at both of its limits: `MAX_ITEMS` items filling `MAX_SIZE`
	// bytes. Each item costs an offset int and its header on top of its data.
	const size_t ItemOverhead = sizeof(int) + sizeof(CSnapshotItem);
	const size_t DataSize = CSnapshot::MAX_SIZE - sizeof(CSnapshot) - CSnapshot::MAX_ITEMS * ItemOverhead;
	const size_t ItemSize = (DataSize / CSnapshot::MAX_ITEMS) & ~(sizeof(int) - 1);
	const auto pToBuilder = std::make_unique<CSnapshotBuilder>();
	pToBuilder->Init();
	for(int Id = 0; Id < CSnapshot::MAX_ITEMS - 1; Id++)
		ASSERT_TRUE(pToBuilder->NewItem(NETOBJTYPE_FLAG, Id, pItemData.get(), ItemSize));
	ASSERT_TRUE(pToBuilder->NewItem(NETOBJTYPE_FLAG, CSnapshot::MAX_ITEMS - 1, pItemData.get(), DataSize - ItemSize * (CSnapshot::MAX_ITEMS - 1)));
	const auto pToBuffer = std::make_unique<CSnapshotBuffer>();
	const int ToSize = pToBuilder->Finish(pToBuffer.get());
	ASSERT_EQ(pToBuffer->AsSnapshot()->NumItems(), CSnapshot::MAX_ITEMS);
	ASSERT_EQ(ToSize, CSnapshot::MAX_SIZE);

	const auto pDelta = std::make_unique<CSnapshotDelta>();
	const auto pDeltaData = std::make_unique<CSnapshotDeltaBuffer>();
	const int DeltaSize = pDelta->CreateDelta(pFromBuffer->AsSnapshot(), pToBuffer->AsSnapshot(), pDeltaData.get());
	EXPECT_GT(DeltaSize, CSnapshot::MAX_SIZE);
	EXPECT_LE(DeltaSize, (int)sizeof(CSnapshotDeltaBuffer));

	// The delta is valid, it just does not fit into a snapshot sized buffer.
	const auto pUnpackedBuffer = std::make_unique<CSnapshotBuffer>();
	EXPECT_EQ(pDelta->UnpackDelta(pFromBuffer->AsSnapshot(), pUnpackedBuffer.get(), pDeltaData->m_aData, DeltaSize), ToSize);
	EXPECT_EQ(mem_comp(pUnpackedBuffer->m_aData, pToBuffer->m_aData, ToSize), 0);
}
