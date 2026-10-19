#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

TEST(RenderLayerVisuals, CompactRowQueryEquivalence)
{
	const int Width = 100;
	const int Height = 50;

	std::vector<std::vector<uint32_t>> vvRowDrawnTiles(Height);
	// Row 0: empty
	// Row 1: sparse tiles at 5, 10, 15, 20, 25, 30
	vvRowDrawnTiles[1] = {5, 10, 15, 20, 25, 30};
	// Row 2: contiguous run from 40 to 60
	for(uint32_t x = 40; x <= 60; ++x)
		vvRowDrawnTiles[2].push_back(x);
	// Row 3: empty
	// Row 4: single tile at 99
	vvRowDrawnTiles[4] = {99};

	class COldVisual
	{
	public:
		uint32_t m_Offset = 0;
		bool m_Draw = false;
	};
	std::vector<COldVisual> vOldFlat(Width * Height);

	class CRowVisual
	{
	public:
		uint32_t m_TileOffset = 0;
		uint32_t m_TileCount = 0;
		uint16_t m_MinX = 0;
		uint16_t m_MaxX = 0;
	};
	std::vector<CRowVisual> vRows(Height);
	std::vector<uint16_t> vDrawnX;

	uint32_t RunningTileCount = 0;
	for(int RowY = 0; RowY < Height; ++RowY)
	{
		uint32_t RowStart = (uint32_t)vDrawnX.size();
		size_t TileIdx = 0;
		for(int x = 0; x < Width; ++x)
		{
			vOldFlat[RowY * Width + x].m_Offset = RunningTileCount * 6 * sizeof(uint32_t);
			if(TileIdx < vvRowDrawnTiles[RowY].size() && vvRowDrawnTiles[RowY][TileIdx] == (uint32_t)x)
			{
				vOldFlat[RowY * Width + x].m_Draw = true;
				vDrawnX.push_back((uint16_t)x);
				++RunningTileCount;
				++TileIdx;
			}
		}
		vRows[RowY].m_TileOffset = RowStart;
		vRows[RowY].m_TileCount = (uint32_t)(vDrawnX.size() - RowStart);
		if(vRows[RowY].m_TileCount > 0)
		{
			vRows[RowY].m_MinX = vDrawnX[RowStart];
			vRows[RowY].m_MaxX = vDrawnX.back();
		}
	}

	// Test all combinations of X0, X1 across rows
	for(int RowIndex = 0; RowIndex < 5; ++RowIndex)
	{
		for(int X0 = 0; X0 < Width; ++X0)
		{
			for(int X1 = X0 + 1; X1 <= Width; ++X1)
			{
				const auto &OldStart = vOldFlat[RowIndex * Width + X0];
				const auto &OldEnd = vOldFlat[RowIndex * Width + (X1 - 1)];
				unsigned int OldNumVertices = ((OldEnd.m_Offset - OldStart.m_Offset) / sizeof(unsigned int)) + (OldEnd.m_Draw ? 6u : 0u);
				uint32_t OldByteOffset = OldStart.m_Offset;

				const auto &Row = vRows[RowIndex];
				unsigned int NewNumVertices = 0;
				uint32_t NewByteOffset = 0;
				if(Row.m_TileCount > 0)
				{
					if(Row.m_MinX < (uint16_t)X1 && Row.m_MaxX >= (uint16_t)X0)
					{
						size_t TileIndex = 0;
						size_t Count = 0;

						if(Row.m_MinX >= (uint16_t)X0 && Row.m_MaxX < (uint16_t)X1)
						{
							TileIndex = Row.m_TileOffset;
							Count = Row.m_TileCount;
						}
						else
						{
							const uint16_t *pBegin = &vDrawnX[Row.m_TileOffset];
							const uint16_t *pEnd = pBegin + Row.m_TileCount;

							const uint16_t *pStart = (Row.m_MinX >= (uint16_t)X0) ? pBegin : std::lower_bound(pBegin, pEnd, (uint16_t)X0);
							const uint16_t *pStop = (Row.m_MaxX < (uint16_t)X1) ? pEnd : std::lower_bound(pStart, pEnd, (uint16_t)X1);

							Count = (size_t)(pStop - pStart);
							TileIndex = Row.m_TileOffset + (size_t)(pStart - pBegin);
						}

						if(Count > 0)
						{
							NewByteOffset = (uint32_t)(TileIndex * (6 * sizeof(unsigned int)));
							NewNumVertices = (unsigned int)(Count * 6);
						}
					}
				}

				EXPECT_EQ(NewNumVertices, OldNumVertices) << "Row: " << RowIndex << " X0: " << X0 << " X1: " << X1;
				if(NewNumVertices > 0)
				{
					EXPECT_EQ(NewByteOffset, OldByteOffset) << "Row: " << RowIndex << " X0: " << X0 << " X1: " << X1;
				}
			}
		}
	}
}

TEST(RenderLayerVisuals, ContiguousRowMerging)
{
	class CRowVisual
	{
	public:
		uint32_t m_TileOffset = 0;
		uint32_t m_TileCount = 0;
		uint16_t m_MinX = 0;
		uint16_t m_MaxX = 0;
	};

	// 5 rows:
	// Row 0: tiles at X in [10..20] (11 tiles, offset 0..10)
	// Row 1: tiles at X in [10..20] (11 tiles, offset 11..21) -> contiguous with row 0
	// Row 2: empty (0 tiles) -> skipped
	// Row 3: tiles at X in [30..40] (11 tiles, offset 22..32) -> contiguous with row 1 in GPU buffer
	// Row 4: tiles at X in [10..20] and [80..90] (22 tiles, offset 33..54) -> partial visibility cuts off [80..90]
	const std::vector<CRowVisual> vRows = {
		{0, 11, 10, 20},
		{11, 11, 10, 20},
		{22, 0, 0, 0},
		{22, 11, 30, 40},
		{33, 22, 10, 90},
	};

	std::vector<uint16_t> vDrawnX;
	for(uint32_t x = 10; x <= 20; ++x)
		vDrawnX.push_back((uint16_t)x);
	for(uint32_t x = 10; x <= 20; ++x)
		vDrawnX.push_back((uint16_t)x);
	for(uint32_t x = 30; x <= 40; ++x)
		vDrawnX.push_back((uint16_t)x);
	for(uint32_t x = 10; x <= 20; ++x)
		vDrawnX.push_back((uint16_t)x);
	for(uint32_t x = 80; x <= 90; ++x)
		vDrawnX.push_back((uint16_t)x);

	// Case 1: Viewport [5..50] over rows 0..3
	// Rows 0, 1, 3 are all within [5..50], Row 2 is empty.
	// In the GPU index buffer, Row 0 (0..10), Row 1 (11..21), Row 3 (22..32) are directly contiguous (33 tiles total).
	{
		const int X0 = 5, X1 = 50;
		size_t ExpectedNextTile = 0;
		std::vector<uint32_t> vDrawCounts;
		std::vector<uint32_t> vByteOffsets;

		for(size_t RowIndex = 0; RowIndex < 4; ++RowIndex)
		{
			const auto &Row = vRows[RowIndex];
			if(Row.m_TileCount == 0)
				continue;

			if(Row.m_MinX >= (uint16_t)X1 || Row.m_MaxX < (uint16_t)X0)
				continue;

			size_t TileIndex = 0;
			size_t Count = 0;

			if(Row.m_MinX >= (uint16_t)X0 && Row.m_MaxX < (uint16_t)X1)
			{
				TileIndex = Row.m_TileOffset;
				Count = Row.m_TileCount;
			}
			else
			{
				const uint16_t *pBegin = &vDrawnX[Row.m_TileOffset];
				const uint16_t *pEnd = pBegin + Row.m_TileCount;

				const uint16_t *pStart = (Row.m_MinX >= (uint16_t)X0) ? pBegin : std::lower_bound(pBegin, pEnd, (uint16_t)X0);
				const uint16_t *pStop = (Row.m_MaxX < (uint16_t)X1) ? pEnd : std::lower_bound(pStart, pEnd, (uint16_t)X1);

				Count = (size_t)(pStop - pStart);
				TileIndex = Row.m_TileOffset + (size_t)(pStart - pBegin);
			}

			if(Count > 0)
			{
				uint32_t NumVertices = (uint32_t)(Count * 6);
				uint32_t ByteOffset = (uint32_t)(TileIndex * 6 * sizeof(uint32_t));

				if(!vDrawCounts.empty() && TileIndex == ExpectedNextTile)
				{
					vDrawCounts.back() += NumVertices;
				}
				else
				{
					vByteOffsets.push_back(ByteOffset);
					vDrawCounts.push_back(NumVertices);
				}
				ExpectedNextTile = TileIndex + Count;
			}
		}

		ASSERT_EQ(vDrawCounts.size(), 1u);
		EXPECT_EQ(vDrawCounts[0], 33u * 6u);
		EXPECT_EQ(vByteOffsets[0], 0u);
	}

	// Case 2: Viewport [5..50] including row 4
	// In row 4, only tiles [10..20] (11 tiles) are visible, tiles [80..90] are offscreen.
	// Row 4's visible tiles start at TileIndex 33, exactly matching ExpectedNextTile (33).
	// So it merges into a single range of 44 tiles (264 vertices).
	{
		const int X0 = 5, X1 = 50;
		size_t ExpectedNextTile = 0;
		std::vector<uint32_t> vDrawCounts;

		for(size_t RowIndex = 0; RowIndex < 5; ++RowIndex)
		{
			const auto &Row = vRows[RowIndex];
			if(Row.m_TileCount == 0)
				continue;

			if(Row.m_MinX >= (uint16_t)X1 || Row.m_MaxX < (uint16_t)X0)
				continue;

			size_t TileIndex = 0;
			size_t Count = 0;

			if(Row.m_MinX >= (uint16_t)X0 && Row.m_MaxX < (uint16_t)X1)
			{
				TileIndex = Row.m_TileOffset;
				Count = Row.m_TileCount;
			}
			else
			{
				const uint16_t *pBegin = &vDrawnX[Row.m_TileOffset];
				const uint16_t *pEnd = pBegin + Row.m_TileCount;

				const uint16_t *pStart = (Row.m_MinX >= (uint16_t)X0) ? pBegin : std::lower_bound(pBegin, pEnd, (uint16_t)X0);
				const uint16_t *pStop = (Row.m_MaxX < (uint16_t)X1) ? pEnd : std::lower_bound(pStart, pEnd, (uint16_t)X1);

				Count = (size_t)(pStop - pStart);
				TileIndex = Row.m_TileOffset + (size_t)(pStart - pBegin);
			}

			if(Count > 0)
			{
				uint32_t NumVertices = (uint32_t)(Count * 6);

				if(!vDrawCounts.empty() && TileIndex == ExpectedNextTile)
				{
					vDrawCounts.back() += NumVertices;
				}
				else
				{
					vDrawCounts.push_back(NumVertices);
				}
				ExpectedNextTile = TileIndex + Count;
			}
		}

		ASSERT_EQ(vDrawCounts.size(), 1u);
		EXPECT_EQ(vDrawCounts[0], 44u * 6u);
	}
}
