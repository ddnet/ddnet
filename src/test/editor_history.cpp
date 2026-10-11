#include "test.h"

#include <base/hash.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/editor/history/tile_plane.h>
#include <game/editor/history/tile_projection.h>
#include <game/editor/mapitems/envelope.h>
#include <game/map/render_map.h>
#include <game/mapitems_ex.h>

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace editor_history;

namespace
{

	class CTestTile
	{
	public:
		int m_Index = 0;
		int m_Flags = 0;
		bool operator==(const CTestTile &) const = default;
	};

	using CTestPlane = CTilePlane<CTestTile, 16>;

} // namespace

TEST(EditorHistoryStorage, SharedValuesCompareContentsAndDetach)
{
	CSharedVector<int> Original(std::vector<int>{1, 2, 3});
	auto Draft = Original;
	EXPECT_TRUE(Draft.SharesStorageWith(Original));
	Draft.Update([](auto &vValues) { vValues[1] = 7; });
	EXPECT_FALSE(Draft.SharesStorageWith(Original));
	EXPECT_EQ(Original.Read()[1], 2);
	EXPECT_EQ(Draft.Read()[1], 7);
	Draft.Update([](auto &vValues) { vValues[1] = 2; });
	EXPECT_EQ(Draft, Original);
	CStorageUsage Usage;
	EXPECT_TRUE(Original.Account(Usage));
	const auto Bytes = Usage.Bytes();
	auto Shared = Original; // NOLINT(performance-unnecessary-copy-initialization): Test owning copies and detachment.
	EXPECT_FALSE(Shared.Account(Usage));
	EXPECT_EQ(Usage.Bytes(), Bytes);
}

TEST(EditorHistoryStorage, SparseEditCopiesOnlyTouchedChunks)
{
	auto Baseline = *CTestPlane::Create(1024, 1024);
	auto Draft = Baseline;
	Draft.Set(17, 19, {3, 4});
	Draft.Set(18, 19, {5, 6});
	EXPECT_EQ(Draft.SharedChunks(Baseline), Baseline.ChunkCount() - 1);
	EXPECT_EQ(Baseline.At(17, 19), CTestTile{});
	EXPECT_EQ(Draft.At(17, 19), (CTestTile{3, 4}));
	auto Frozen = Draft;
	Draft.Set(17, 19, {7, 8});
	EXPECT_EQ(Frozen.At(17, 19), (CTestTile{3, 4}));
}

TEST(EditorHistoryStorage, ResizePreservesCoordinatesAndClearsRemovedCells)
{
	auto Plane = *CTestPlane::Create(35, 21);
	Plane.Set(4, 5, {11, 2});
	Plane.Set(34, 20, {27, 3});
	const auto Baseline = Plane;
	ASSERT_TRUE(Plane.Resize(65, 40));
	EXPECT_EQ(Plane.At(34, 20), (CTestTile{27, 3}));
	EXPECT_EQ(Plane.At(35, 20), CTestTile{});
	EXPECT_EQ(Plane.SharedChunks(Baseline), Baseline.ChunkCount());
	ASSERT_TRUE(Plane.Resize(20, 18));
	ASSERT_TRUE(Plane.Resize(65, 40));
	EXPECT_EQ(Plane.At(4, 5), (CTestTile{11, 2}));
	EXPECT_EQ(Plane.At(34, 20), CTestTile{});
	EXPECT_EQ(Baseline.At(34, 20), (CTestTile{27, 3}));
}

TEST(EditorHistoryStorage, OverflowDoesNotModifyPlane)
{
	auto Plane = *CTestPlane::Create(32, 32);
	Plane.Set(1, 2, {5, 6});
	const auto Baseline = Plane;
	EXPECT_FALSE(CTestPlane::Create(std::numeric_limits<std::size_t>::max(), 1));
	EXPECT_FALSE(Plane.Resize(1, std::numeric_limits<std::size_t>::max()));
	EXPECT_EQ(Plane, Baseline);
}

TEST(EditorHistoryStorage, IncrementalUnionMatchesFreshAccountingAndRollsBack)
{
	auto First = *CTestPlane::Create(32768, 32768);
	First.Set(0, 0, {1, 2});
	auto Second = First;
	Second.Set(32767, 32767, {3, 4});
	CStorageUsage Incremental;
	First.Account(Incremental);
	const auto FirstBytes = Incremental.Bytes();
	{
		CStorageUsage::CTransaction Transaction(Incremental);
		Second.Account(Incremental);
		CStorageUsage Fresh;
		First.Account(Fresh);
		Second.Account(Fresh);
		EXPECT_EQ(Incremental.Bytes(), Fresh.Bytes());
		Incremental.Removing(true);
		First.Account(Incremental);
		CStorageUsage Remaining;
		Second.Account(Remaining);
		EXPECT_EQ(Incremental.Bytes(), Remaining.Bytes());
		// Simulates a later allocation failure: destruction rolls back both paths.
	}
	EXPECT_EQ(Incremental.Bytes(), FirstBytes);
	{
		CStorageUsage::CTransaction Transaction(Incremental);
		Second.Account(Incremental);
		Incremental.Removing(true);
		First.Account(Incremental);
		Transaction.Commit();
	}
	CStorageUsage Remaining;
	Second.Account(Remaining);
	EXPECT_EQ(Incremental.Bytes(), Remaining.Bytes());
	{
		CStorageUsage::CTransaction Transaction(Incremental);
		Incremental.Removing(true);
		Second.Account(Incremental);
		Transaction.Commit();
	}
	EXPECT_EQ(Incremental.Bytes(), 0u);
	EXPECT_EQ(Incremental.Allocations(), 0u);
}
