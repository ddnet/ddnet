#ifndef GAME_SPARSE_LAYER_H
#define GAME_SPARSE_LAYER_H

#include <base/mem.h>
#include <base/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T, int BlockShift = 4>
class CSparseTileLayer
{
	static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
	static constexpr int BLOCK_DIM = 1 << BlockShift; // 16
	static constexpr int BLOCK_MASK = BLOCK_DIM - 1; // 15
	static constexpr int BLOCK_TILES = BLOCK_DIM * BLOCK_DIM; // 256
	static constexpr size_t BLOCK_BYTES = BLOCK_TILES * sizeof(T);

private:
	int m_Width = 0;
	int m_Height = 0;
	int m_BlocksX = 0;
	int m_BlocksY = 0;

	class CBlock
	{
	public:
		T m_aTiles[BLOCK_TILES];
	};
	std::vector<CBlock> m_vStorage;
	std::vector<const T *> m_vpBlockPtrs;

	inline static const CBlock ms_ZeroBlock{};

public:
	CSparseTileLayer() = default;

	CSparseTileLayer(const CSparseTileLayer &) = delete;
	CSparseTileLayer &operator=(const CSparseTileLayer &) = delete;

	CSparseTileLayer(CSparseTileLayer &&Other) noexcept :
		m_Width(std::exchange(Other.m_Width, 0)),
		m_Height(std::exchange(Other.m_Height, 0)),
		m_BlocksX(std::exchange(Other.m_BlocksX, 0)),
		m_BlocksY(std::exchange(Other.m_BlocksY, 0)),
		m_vStorage(std::move(Other.m_vStorage)),
		m_vpBlockPtrs(std::move(Other.m_vpBlockPtrs))
	{
	}

	CSparseTileLayer &operator=(CSparseTileLayer &&Other) noexcept
	{
		if(this != &Other)
		{
			m_Width = std::exchange(Other.m_Width, 0);
			m_Height = std::exchange(Other.m_Height, 0);
			m_BlocksX = std::exchange(Other.m_BlocksX, 0);
			m_BlocksY = std::exchange(Other.m_BlocksY, 0);
			m_vStorage = std::move(Other.m_vStorage);
			m_vpBlockPtrs = std::move(Other.m_vpBlockPtrs);
		}
		return *this;
	}

	void Clear()
	{
		m_Width = 0;
		m_Height = 0;
		m_BlocksX = 0;
		m_BlocksY = 0;
		m_vStorage.clear();
		m_vStorage.shrink_to_fit();
		m_vpBlockPtrs.clear();
		m_vpBlockPtrs.shrink_to_fit();
	}

	bool HasData() const { return !m_vpBlockPtrs.empty(); }
	int Width() const { return m_Width; }
	int Height() const { return m_Height; }
	size_t NonEmptyBlocks() const { return m_vStorage.size(); }
	size_t TotalBlocks() const { return m_vpBlockPtrs.size(); }

	template<typename FPredicate>
	void InitFromRaw(const T *pRawData, int Width, int Height, FPredicate &&IsEmptyTile)
	{
		Clear();
		if(!pRawData || Width <= 0 || Height <= 0)
			return;

		m_Width = Width;
		m_Height = Height;
		m_BlocksX = (Width + BLOCK_DIM - 1) >> BlockShift;
		m_BlocksY = (Height + BLOCK_DIM - 1) >> BlockShift;
		const int TotalBlocksCount = m_BlocksX * m_BlocksY;

		// First pass: identify non-empty blocks
		std::vector<bool> vBlockHasData(TotalBlocksCount, false);
		size_t NonEmptyCount = 0;

		for(int BlockY = 0; BlockY < m_BlocksY; ++BlockY)
		{
			const int StartY = BlockY << BlockShift;
			const int EndY = std::min(StartY + BLOCK_DIM, m_Height);

			for(int BlockX = 0; BlockX < m_BlocksX; ++BlockX)
			{
				const int StartX = BlockX << BlockShift;
				const int EndX = std::min(StartX + BLOCK_DIM, m_Width);
				const int BlockIdx = BlockY * m_BlocksX + BlockX;

				bool HasNonZero = false;
				for(int TileY = StartY; TileY < EndY && !HasNonZero; ++TileY)
				{
					for(int TileX = StartX; TileX < EndX; ++TileX)
					{
						const T &Tile = pRawData[TileY * m_Width + TileX];
						if(!IsEmptyTile(Tile))
						{
							HasNonZero = true;
							break;
						}
					}
				}

				if(HasNonZero)
				{
					vBlockHasData[BlockIdx] = true;
					NonEmptyCount++;
				}
			}
		}

		m_vStorage.resize(NonEmptyCount);
		m_vpBlockPtrs.resize(TotalBlocksCount, ms_ZeroBlock.m_aTiles);

		size_t StorageIdx = 0;
		for(int BlockY = 0; BlockY < m_BlocksY; ++BlockY)
		{
			const int StartY = BlockY << BlockShift;
			const int EndY = std::min(StartY + BLOCK_DIM, m_Height);

			for(int BlockX = 0; BlockX < m_BlocksX; ++BlockX)
			{
				const int BlockIdx = BlockY * m_BlocksX + BlockX;
				if(!vBlockHasData[BlockIdx])
					continue;

				CBlock &Block = m_vStorage[StorageIdx++];
				Block = {};

				const int StartX = BlockX << BlockShift;
				const int EndX = std::min(StartX + BLOCK_DIM, m_Width);

				for(int TileY = StartY; TileY < EndY; ++TileY)
				{
					const int LocalY = TileY - StartY;
					for(int TileX = StartX; TileX < EndX; ++TileX)
					{
						const int LocalX = TileX - StartX;
						Block.m_aTiles[(LocalY << BlockShift) | LocalX] = pRawData[TileY * m_Width + TileX];
					}
				}

				m_vpBlockPtrs[BlockIdx] = Block.m_aTiles;
			}
		}
	}

	void InitFromRaw(const T *pRawData, int Width, int Height)
	{
		InitFromRaw(pRawData, Width, Height, [](const T &Tile) {
			return mem_is_null(&Tile, sizeof(T));
		});
	}

	const T &At(int X, int Y) const
	{
		if(X < 0 || X >= m_Width || Y < 0 || Y >= m_Height || m_vpBlockPtrs.empty())
			return ms_ZeroBlock.m_aTiles[0];

		const int BlockX = X >> BlockShift;
		const int BlockY = Y >> BlockShift;
		const T *pBlock = m_vpBlockPtrs[BlockY * m_BlocksX + BlockX];
		const int LocalX = X & BLOCK_MASK;
		const int LocalY = Y & BLOCK_MASK;
		return pBlock[(LocalY << BlockShift) | LocalX];
	}

	const T &At(int Index) const
	{
		if(Index < 0 || Index >= m_Width * m_Height || m_vpBlockPtrs.empty())
			return ms_ZeroBlock.m_aTiles[0];

		const int X = Index % m_Width;
		const int Y = Index / m_Width;
		const int BlockX = X >> BlockShift;
		const int BlockY = Y >> BlockShift;
		const T *pBlock = m_vpBlockPtrs[BlockY * m_BlocksX + BlockX];
		const int LocalX = X & BLOCK_MASK;
		const int LocalY = Y & BLOCK_MASK;
		return pBlock[(LocalY << BlockShift) | LocalX];
	}

	const T &Tile(int X, int Y) const { return At(X, Y); }
	const T &Tile(int Index) const { return At(Index); }
	const T &operator()(int X, int Y) const { return At(X, Y); }
	const T &operator[](int Index) const { return At(Index); }

	template<typename FFunc, typename FPredicate>
	void ForEachNonZero(FFunc &&Func, FPredicate &&IsEmptyTile) const
	{
		if(m_vpBlockPtrs.empty())
			return;

		for(int BlockY = 0; BlockY < m_BlocksY; ++BlockY)
		{
			const int StartY = BlockY << BlockShift;
			const int EndY = std::min(StartY + BLOCK_DIM, m_Height);

			for(int BlockX = 0; BlockX < m_BlocksX; ++BlockX)
			{
				const T *pBlock = m_vpBlockPtrs[BlockY * m_BlocksX + BlockX];
				if(pBlock == ms_ZeroBlock.m_aTiles)
					continue;

				const int StartX = BlockX << BlockShift;
				const int EndX = std::min(StartX + BLOCK_DIM, m_Width);

				for(int TileY = StartY; TileY < EndY; ++TileY)
				{
					const int LocalY = TileY - StartY;
					for(int TileX = StartX; TileX < EndX; ++TileX)
					{
						const int LocalX = TileX - StartX;
						const T &Tile = pBlock[(LocalY << BlockShift) | LocalX];
						if(!IsEmptyTile(Tile))
						{
							const int Index = TileY * m_Width + TileX;
							Func(TileX, TileY, Index, Tile);
						}
					}
				}
			}
		}
	}

	template<typename FFunc>
	void ForEachNonZero(FFunc &&Func) const
	{
		ForEachNonZero(std::forward<FFunc>(Func), [](const T &Tile) {
			return mem_is_null(&Tile, sizeof(T));
		});
	}
};

#endif
