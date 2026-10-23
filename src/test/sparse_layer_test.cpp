#include <game/collision.h>
#include <game/mapitems.h>
#include <game/sparse_layer.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>
#include <vector>

TEST(SparseLayer, BasicOperations)
{
	CSparseTileLayer<CTile, 4> Layer;
	EXPECT_FALSE(Layer.HasData());
	EXPECT_EQ(Layer.Width(), 0);
	EXPECT_EQ(Layer.Height(), 0);
	EXPECT_EQ(Layer.NonEmptyBlocks(), 0u);

	// Out of bounds on uninitialized layer returns zero tile
	CTile Empty = Layer.At(5, 5);
	EXPECT_EQ(Empty.m_Index, 0);

	// Create a 64x64 raw layer (4x4 blocks of 16x16)
	const int Width = 64;
	const int Height = 64;
	std::vector<CTile> vRaw(Width * Height);

	// Place tiles in Block (0, 0) and Block (2, 3)
	// Block (0, 0): tile at (5, 5)
	vRaw[5 * Width + 5].m_Index = TILE_SOLID;
	vRaw[5 * Width + 5].m_Flags = 1;

	// Block (2, 3): BlockX=2 => X in [32, 47], BlockY=3 => Y in [48, 63]
	vRaw[50 * Width + 35].m_Index = TILE_DEATH;
	vRaw[50 * Width + 35].m_Flags = 2;

	Layer.InitFromRaw(vRaw.data(), Width, Height);

	EXPECT_TRUE(Layer.HasData());
	EXPECT_EQ(Layer.Width(), 64);
	EXPECT_EQ(Layer.Height(), 64);
	EXPECT_EQ(Layer.TotalBlocks(), 16u);
	EXPECT_EQ(Layer.NonEmptyBlocks(), 2u);

	// Read populated tiles
	EXPECT_EQ(Layer.At(5, 5).m_Index, TILE_SOLID);
	EXPECT_EQ(Layer.At(5, 5).m_Flags, 1);
	EXPECT_EQ(Layer.Tile(5, 5).m_Index, TILE_SOLID);
	EXPECT_EQ(Layer(5, 5).m_Index, TILE_SOLID);
	EXPECT_EQ(Layer.At(5 * Width + 5).m_Index, TILE_SOLID);
	EXPECT_EQ(Layer[5 * Width + 5].m_Index, TILE_SOLID);

	EXPECT_EQ(Layer.At(35, 50).m_Index, TILE_DEATH);
	EXPECT_EQ(Layer.At(35, 50).m_Flags, 2);
	EXPECT_EQ(Layer.At(50 * Width + 35).m_Index, TILE_DEATH);

	// Read empty tile in non-empty block
	EXPECT_EQ(Layer.At(6, 5).m_Index, 0);

	// Read empty tile in empty block
	EXPECT_EQ(Layer.At(20, 20).m_Index, 0);

	// Out of bounds queries
	EXPECT_EQ(Layer.At(-1, 0).m_Index, 0);
	EXPECT_EQ(Layer.At(0, -1).m_Index, 0);
	EXPECT_EQ(Layer.At(64, 0).m_Index, 0);
	EXPECT_EQ(Layer.At(0, 64).m_Index, 0);
	EXPECT_EQ(Layer.At(-100).m_Index, 0);
	EXPECT_EQ(Layer.At(Width * Height + 10).m_Index, 0);

	// ForEachNonZero
	int Count = 0;
	Layer.ForEachNonZero([&](int X, int Y, int Index, const CTile &Tile) {
		Count++;
		if(X == 5 && Y == 5)
		{
			EXPECT_EQ(Tile.m_Index, TILE_SOLID);
		}
		else if(X == 35 && Y == 50)
		{
			EXPECT_EQ(Tile.m_Index, TILE_DEATH);
		}
		else
		{
			FAIL() << "Unexpected non-zero tile at (" << X << ", " << Y << ")";
		}
	});
	EXPECT_EQ(Count, 2);

	// Clear
	Layer.Clear();
	EXPECT_FALSE(Layer.HasData());
	EXPECT_EQ(Layer.NonEmptyBlocks(), 0u);
}

TEST(SparseLayer, MoveSemantics)
{
	static_assert(!std::is_copy_constructible_v<CSparseTileLayer<CTile>>);
	static_assert(!std::is_copy_assignable_v<CSparseTileLayer<CTile>>);
	static_assert(std::is_move_constructible_v<CSparseTileLayer<CTile>>);
	static_assert(std::is_move_assignable_v<CSparseTileLayer<CTile>>);

	const int Width = 32;
	const int Height = 32;
	std::vector<CTile> vRaw(Width * Height);
	vRaw[5 * Width + 5].m_Index = TILE_SOLID;

	CSparseTileLayer<CTile, 4> SourceLayer;
	SourceLayer.InitFromRaw(vRaw.data(), Width, Height);
	EXPECT_TRUE(SourceLayer.HasData());
	EXPECT_EQ(SourceLayer.At(5, 5).m_Index, TILE_SOLID);

	// Move constructor
	CSparseTileLayer<CTile, 4> MoveConstructed(std::move(SourceLayer));
	EXPECT_TRUE(MoveConstructed.HasData());
	EXPECT_EQ(MoveConstructed.Width(), Width);
	EXPECT_EQ(MoveConstructed.Height(), Height);
	EXPECT_EQ(MoveConstructed.At(5, 5).m_Index, TILE_SOLID);
	EXPECT_FALSE(SourceLayer.HasData()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	EXPECT_EQ(SourceLayer.Width(), 0); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)

	// Move assignment
	CSparseTileLayer<CTile, 4> MoveAssigned;
	MoveAssigned = std::move(MoveConstructed);
	EXPECT_TRUE(MoveAssigned.HasData());
	EXPECT_EQ(MoveAssigned.Width(), Width);
	EXPECT_EQ(MoveAssigned.Height(), Height);
	EXPECT_EQ(MoveAssigned.At(5, 5).m_Index, TILE_SOLID);
	EXPECT_FALSE(MoveConstructed.HasData()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	EXPECT_EQ(MoveConstructed.Width(), 0); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

TEST(SparseLayer, CustomTileTypes)
{
	// CSpeedupTile
	{
		CSparseTileLayer<CSpeedupTile, 4> SpeedupLayer;
		const int Width = 32;
		const int Height = 32;
		std::vector<CSpeedupTile> vSpeedup(Width * Height);

		vSpeedup[10 * Width + 10].m_Force = 5;
		vSpeedup[10 * Width + 10].m_MaxSpeed = 10;
		vSpeedup[10 * Width + 10].m_Angle = 90;

		SpeedupLayer.InitFromRaw(vSpeedup.data(), Width, Height);
		EXPECT_EQ(SpeedupLayer.NonEmptyBlocks(), 1u);
		EXPECT_EQ(SpeedupLayer.At(10, 10).m_Force, 5);
		EXPECT_EQ(SpeedupLayer.At(10, 10).m_MaxSpeed, 10);
		EXPECT_EQ(SpeedupLayer.At(10, 10).m_Angle, 90);
		EXPECT_EQ(SpeedupLayer.At(0, 0).m_Force, 0);
	}

	// CTeleTile
	{
		CSparseTileLayer<CTeleTile, 4> TeleLayer;
		const int Width = 32;
		const int Height = 32;
		std::vector<CTeleTile> vTele(Width * Height);

		vTele[15 * Width + 20].m_Number = 7;
		vTele[15 * Width + 20].m_Type = TILE_TELEIN;

		TeleLayer.InitFromRaw(vTele.data(), Width, Height);
		EXPECT_EQ(TeleLayer.NonEmptyBlocks(), 1u);
		EXPECT_EQ(TeleLayer.At(20, 15).m_Number, 7);
		EXPECT_EQ(TeleLayer.At(20, 15).m_Type, TILE_TELEIN);
		EXPECT_EQ(TeleLayer.At(0, 0).m_Number, 0);
	}

	// CSwitchTile
	{
		CSparseTileLayer<CSwitchTile, 4> SwitchLayer;
		const int Width = 32;
		const int Height = 32;
		std::vector<CSwitchTile> vSwitch(Width * Height);

		vSwitch[2 * Width + 3].m_Number = 42;
		vSwitch[2 * Width + 3].m_Type = TILE_SWITCHTIMEDOPEN;
		vSwitch[2 * Width + 3].m_Flags = 1;
		vSwitch[2 * Width + 3].m_Delay = 5;

		SwitchLayer.InitFromRaw(vSwitch.data(), Width, Height);
		EXPECT_EQ(SwitchLayer.NonEmptyBlocks(), 1u);
		EXPECT_EQ(SwitchLayer.At(3, 2).m_Number, 42);
		EXPECT_EQ(SwitchLayer.At(3, 2).m_Type, TILE_SWITCHTIMEDOPEN);
		EXPECT_EQ(SwitchLayer.At(3, 2).m_Flags, 1);
		EXPECT_EQ(SwitchLayer.At(3, 2).m_Delay, 5);
	}

	// CTuneTile
	{
		CSparseTileLayer<CTuneTile, 4> TuneLayer;
		const int Width = 32;
		const int Height = 32;
		std::vector<CTuneTile> vTune(Width * Height);

		vTune[1 * Width + 1].m_Number = 3;
		vTune[1 * Width + 1].m_Type = 1;

		TuneLayer.InitFromRaw(vTune.data(), Width, Height);
		EXPECT_EQ(TuneLayer.NonEmptyBlocks(), 1u);
		EXPECT_EQ(TuneLayer.At(1, 1).m_Number, 3);
		EXPECT_EQ(TuneLayer.At(1, 1).m_Type, 1);
	}
}

TEST(SparseLayer, NonMultipleOfBlockDimensions)
{
	// Non-power-of-two / non-multiple-of-16 grid: 25x19
	const int Width = 25;
	const int Height = 19;
	// BlocksX = ceil(25/16) = 2, BlocksY = ceil(19/16) = 2 -> 4 blocks total
	CSparseTileLayer<CTile, 4> Layer;
	std::vector<CTile> vRaw(Width * Height);

	// Tile at boundary edge: (24, 18)
	vRaw[18 * Width + 24].m_Index = TILE_FREEZE;

	Layer.InitFromRaw(vRaw.data(), Width, Height);
	EXPECT_EQ(Layer.Width(), 25);
	EXPECT_EQ(Layer.Height(), 19);
	EXPECT_EQ(Layer.TotalBlocks(), 4u);
	EXPECT_EQ(Layer.NonEmptyBlocks(), 1u);

	EXPECT_EQ(Layer.At(24, 18).m_Index, TILE_FREEZE);
	EXPECT_EQ(Layer.At(25, 18).m_Index, 0); // Out of bounds
	EXPECT_EQ(Layer.At(24, 19).m_Index, 0); // Out of bounds
}

TEST(SparseLayer, CustomPredicate)
{
	const int Width = 32;
	const int Height = 32;
	std::vector<CTile> vRaw(Width * Height);

	// Set tile to TILE_AIR (0), but with flags = 99 (treated as empty by custom predicate)
	vRaw[2 * Width + 2].m_Index = 0;
	vRaw[2 * Width + 2].m_Flags = 99;

	// Set tile to TILE_SOLID (not empty)
	vRaw[3 * Width + 3].m_Index = TILE_SOLID;

	CSparseTileLayer<CTile, 4> Layer;
	Layer.InitFromRaw(vRaw.data(), Width, Height, [](const CTile &Tile) {
		return Tile.m_Index == 0;
	});

	EXPECT_TRUE(Layer.HasData());
	EXPECT_EQ(Layer.At(3, 3).m_Index, TILE_SOLID);

	int Count = 0;
	Layer.ForEachNonZero(
		[&](int X, int Y, int Index, const CTile &Tile) {
			Count++;
			EXPECT_EQ(Tile.m_Index, TILE_SOLID);
		},
		[](const CTile &Tile) {
			return Tile.m_Index == 0;
		});
	EXPECT_EQ(Count, 1);
}
