#ifndef GAME_EDITOR_MAPITEMS_TILE_VALUES_H
#define GAME_EDITOR_MAPITEMS_TILE_VALUES_H

#include <base/dbg.h>
#include <base/detect.h>

#include <game/editor/history/tile_plane.h>
#include <game/editor/history/tile_projection.h>
#include <game/mapitems.h>

#include <cstdint>
#include <cstring>
#include <span>

#if defined(CONF_ARCH_AMD64)
#include <emmintrin.h>
#endif

/** Semantic cells have no file padding, skip counts, or derived opacity. */
class CTileValues
{
public:
	unsigned char m_Index = 0;
	unsigned char m_Flags = 0;
	CTileValues() = default;
	CTileValues(CTile Tile) :
		m_Index(Tile.m_Index), m_Flags(Tile.m_Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE)) {}
	void Normalize() { m_Flags &= TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE; }
	operator CTile() const { return {m_Index, m_Flags, 0, 0}; }
	bool operator==(const CTileValues &) const = default;
};

class CTeleTileValues
{
public:
	unsigned char m_Number = 0;
	unsigned char m_Type = 0;
	CTeleTileValues() = default;
	CTeleTileValues(CTeleTile Tile) :
		m_Number(Tile.m_Number), m_Type(Tile.m_Type) {}
	operator CTeleTile() const { return {m_Number, m_Type}; }
	bool operator==(const CTeleTileValues &) const = default;
};

class CSpeedupTileValues
{
public:
	unsigned char m_Force = 0;
	unsigned char m_MaxSpeed = 0;
	unsigned char m_Type = 0;
	short m_Angle = 0;
	CSpeedupTileValues() = default;
	CSpeedupTileValues(CSpeedupTile Tile) :
		m_Force(Tile.m_Force), m_MaxSpeed(Tile.m_MaxSpeed), m_Type(Tile.m_Type), m_Angle(Tile.m_Angle) {}
	operator CSpeedupTile() const { return {m_Force, m_MaxSpeed, m_Type, 0, m_Angle}; }
	bool operator==(const CSpeedupTileValues &) const = default;
};

class CSwitchTileValues
{
public:
	unsigned char m_Number = 0;
	unsigned char m_Type = 0;
	unsigned char m_Flags = 0;
	unsigned char m_Delay = 0;
	CSwitchTileValues() = default;
	CSwitchTileValues(CSwitchTile Tile) :
		m_Number(Tile.m_Number), m_Type(Tile.m_Type), m_Flags(Tile.m_Flags), m_Delay(Tile.m_Delay) {}
	operator CSwitchTile() const { return {m_Number, m_Type, m_Flags, m_Delay}; }
	bool operator==(const CSwitchTileValues &) const = default;
};

class CTuneTileValues
{
public:
	unsigned char m_Number = 0;
	unsigned char m_Type = 0;
	CTuneTileValues() = default;
	CTuneTileValues(CTuneTile Tile) :
		m_Number(Tile.m_Number), m_Type(Tile.m_Type) {}
	operator CTuneTile() const { return {m_Number, m_Type}; }
	bool operator==(const CTuneTileValues &) const = default;
};

/** Shared spatial cells. Mutable references exist only inside Update callbacks. */
template<typename T>
class CEditorTilePlane
{
public:
	static constexpr std::size_t CHUNK_EDGE = 32;
	using CPlane = editor_history::CTilePlane<T, CHUNK_EDGE>;

private:
	CPlane m_Plane;

public:
	CEditorTilePlane() = default;
	CEditorTilePlane(int Width, int Height)
	{
		dbg_assert(Resize(Width, Height), "Invalid editor tile plane dimensions (%d x %d)", Width, Height);
	}

	int Width() const { return static_cast<int>(m_Plane.Width()); }
	int Height() const { return static_cast<int>(m_Plane.Height()); }
	std::size_t Size() const { return m_Plane.Width() * m_Plane.Height(); }
	const CPlane &Plane() const { return m_Plane; }
	const T &operator[](std::size_t Index) const
	{
		dbg_assert(m_Plane.Width() != 0 && Index < Size(), "Tile index out of bounds");
		return m_Plane.At(Index % m_Plane.Width(), Index / m_Plane.Width());
	}
	typename CPlane::CRowSpan ReadRow(std::size_t X, std::size_t Y, std::size_t EndX) const { return m_Plane.ReadRow(X, Y, EndX); }
	typename CPlane::CRowSpan ReadRowCached(std::size_t X, std::size_t Y, std::size_t EndX, CTileChunkCursor &Cursor) const { return m_Plane.ReadRowCached(X, Y, EndX, Cursor); }

	void Set(std::size_t Index, T Value)
	{
		dbg_assert(m_Plane.Width() != 0 && Index < Size(), "Tile index out of bounds");
		if constexpr(requires { Value.Normalize(); })
			Value.Normalize();
		m_Plane.Set(Index % m_Plane.Width(), Index / m_Plane.Width(), Value);
	}

	template<typename F>
	void Update(std::size_t Index, F &&Function)
	{
		T Value = (*this)[Index];
		static_assert(std::is_void_v<std::invoke_result_t<F, T &>>);
		std::invoke(std::forward<F>(Function), Value);
		Set(Index, Value);
	}

	bool Resize(int Width, int Height)
	{
		return Width >= 0 && Height >= 0 && m_Plane.Resize(Width, Height);
	}

	template<typename TMap>
	void Assign(std::span<const TMap> Cells)
	{
		dbg_assert(Cells.size() == Size(), "Tile input size does not match plane dimensions");
		const auto *pCells = Cells.data();
		const auto RowStride = Width();
		T Empty(TMap{});
		if constexpr(requires { Empty.Normalize(); })
			Empty.Normalize();
		const bool EmptyMapsToEmpty = Empty == T{};
		m_Plane.AssignRows([&](std::size_t X, std::size_t Y, std::span<T> Row) {
			const auto *pInput = pCells + Y * RowStride + X;
			auto *pOutput = Row.data();
			const auto Count = Row.size();
			if constexpr(std::is_trivially_copyable_v<TMap>)
			{
				// The destination chunk starts empty. Padding/skip differences only
				// miss this shortcut and use the semantic conversion below.
				static const std::array<TMap, CHUNK_EDGE> s_aEmpty{};
				if(EmptyMapsToEmpty && std::memcmp(pInput, s_aEmpty.data(), Count * sizeof(TMap)) == 0)
					return std::size_t{0};
			}
			std::size_t Index = 0, Nonzero = 0;
#if defined(CONF_ARCH_AMD64)
			// SSE2 is part of the AMD64 ABI; other architectures use the scalar loop below.
			// NOLINTBEGIN(portability-simd-intrinsics)
			if constexpr(std::is_same_v<T, CTileValues> && std::is_same_v<TMap, CTile>)
			{
				// Four ABI tiles become four semantic cells. Reserved/skip/opacity
				// bytes never participate, including when the tile index is zero.
				static_assert(sizeof(CTile) == 4 && sizeof(CTileValues) == 2);
				static_assert(std::is_trivially_copyable_v<CTileValues>);
				static_assert(offsetof(CTileValues, m_Index) == 0 && offsetof(CTileValues, m_Flags) == 1);
				const auto Mask = _mm_set1_epi32(0xff | ((TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE) << 8));
				const auto Zero = _mm_setzero_si128();
				auto ZeroCounts = Zero;
				for(; Index + 4 <= Count; Index += 4)
				{
					const auto Values = _mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i *>(pInput + Index)), Mask);
					const auto Packed = _mm_cvtsi128_si64(_mm_packs_epi32(Values, Zero));
					std::memcpy(static_cast<void *>(pOutput + Index), &Packed, sizeof(Packed));
					ZeroCounts = _mm_sub_epi32(ZeroCounts, _mm_cmpeq_epi32(Values, Zero));
				}
				ZeroCounts = _mm_add_epi32(ZeroCounts, _mm_srli_si128(ZeroCounts, 8));
				ZeroCounts = _mm_add_epi32(ZeroCounts, _mm_srli_si128(ZeroCounts, 4));
				Nonzero = Index - static_cast<std::size_t>(_mm_cvtsi128_si32(ZeroCounts));
			}
			// NOLINTEND(portability-simd-intrinsics)
#endif
			for(; Index < Count; ++Index)
			{
				T Value(pInput[Index]);
				if constexpr(requires { Value.Normalize(); })
					Value.Normalize();
				pOutput[Index] = Value;
				Nonzero += !(Value == T{});
			}
			return Nonzero;
		});
	}

	/** Derive stored chunks only; Function must map an empty source to T{}. */
	template<typename U, typename F>
	void AssignSparse(const CEditorTilePlane<U> &Source, F &&Function)
	{
		m_Plane.AssignSparse(Source.Plane(), [&](const U &Cell) {
			T Value = Function(Cell);
			if constexpr(requires { Value.Normalize(); })
				Value.Normalize();
			return Value;
		});
	}

	void FlipX()
	{
		for(int Y = 0; Y < Height(); ++Y)
			for(int X = 0; X < Width() / 2; ++X)
			{
				const std::size_t First = Y * Width() + X;
				const std::size_t Second = (Y + 1) * Width() - 1 - X;
				const T Saved = (*this)[First];
				Set(First, (*this)[Second]);
				Set(Second, Saved);
			}
	}

	void FlipY()
	{
		for(int Y = 0; Y < Height() / 2; ++Y)
			for(int X = 0; X < Width(); ++X)
			{
				const std::size_t First = Y * Width() + X;
				const std::size_t Second = (Height() - 1 - Y) * Width() + X;
				const T Saved = (*this)[First];
				Set(First, (*this)[Second]);
				Set(Second, Saved);
			}
	}

	void RotateClockwise()
	{
		CEditorTilePlane Candidate(Height(), Width());
		for(int Y = 0; Y < Height(); ++Y)
			for(int X = 0; X < Width(); ++X)
				Candidate.Set(X * Height() + Height() - 1 - Y, (*this)[Y * Width() + X]);
		*this = std::move(Candidate);
	}

	void Shift(int OffsetX, int OffsetY)
	{
		CEditorTilePlane Candidate(Width(), Height());
		for(int Y = 0; Y < Height(); ++Y)
			for(int X = 0; X < Width(); ++X)
				if(X + OffsetX >= 0 && X + OffsetX < Width() && Y + OffsetY >= 0 && Y + OffsetY < Height())
					Candidate.Set((Y + OffsetY) * Width() + X + OffsetX, (*this)[Y * Width() + X]);
		*this = std::move(Candidate);
	}

	bool operator==(const CEditorTilePlane &) const = default;
	void Account(editor_history::CStorageUsage &Usage) const { m_Plane.Account(Usage); }
};

/** Cache projection inputs as well as cells: image replacement can change opacity. */
class CEditorTileProjection
{
	editor_history::CTileProjection<CTileValues, CTile, 32> m_Projection;
	std::array<unsigned char, 256> m_aFlags{};

public:
	std::span<const CTile> Cells() const { return m_Projection.Cells(); }
	void Account(editor_history::CStorageUsage &Usage) const { m_Projection.Account(Usage); }
	std::size_t Refresh(const CEditorTilePlane<CTileValues> &Tiles, const std::array<unsigned char, 256> &aFlags)
	{
		if(aFlags != m_aFlags)
		{
			m_Projection.Invalidate();
			m_aFlags = aFlags;
		}
		return m_Projection.Refresh(Tiles.Plane(), [&](const CTileValues &Value) {
			CTile Tile = Value;
			Tile.m_Flags |= aFlags[Tile.m_Index];
			return Tile;
		});
	}
};

inline std::vector<CTile> ExportTilePlane(const CEditorTilePlane<CTileValues> &Tiles, const std::array<unsigned char, 256> &aFlags)
{
	std::vector<CTile> vResult(Tiles.Size());
	for(std::size_t Index = 0; Index < Tiles.Size(); ++Index)
	{
		vResult[Index] = static_cast<CTile>(Tiles[Index]);
		vResult[Index].m_Flags |= aFlags[vResult[Index].m_Index];
	}
	return vResult;
}

inline void RotateTileFlagsClockwise(CTileValues &Tile)
{
	if(Tile.m_Flags & TILEFLAG_ROTATE)
		Tile.m_Flags ^= TILEFLAG_YFLIP | TILEFLAG_XFLIP;
	Tile.m_Flags ^= TILEFLAG_ROTATE;
}

inline void FlipSpeedupTilesX(CEditorTilePlane<CSpeedupTileValues> &Tiles)
{
	Tiles.FlipX();
	for(std::size_t Index = 0; Index < Tiles.Size(); ++Index)
		Tiles.Update(Index, [](auto &Tile) { Tile.m_Angle = (180 - Tile.m_Angle % 360 + 360) % 360; });
}

inline void FlipSpeedupTilesY(CEditorTilePlane<CSpeedupTileValues> &Tiles)
{
	Tiles.FlipY();
	for(std::size_t Index = 0; Index < Tiles.Size(); ++Index)
		Tiles.Update(Index, [](auto &Tile) { Tile.m_Angle = (360 - Tile.m_Angle % 360 + 360) % 360; });
}

inline void RotateSpeedupTilesClockwise(CEditorTilePlane<CSpeedupTileValues> &Tiles)
{
	Tiles.RotateClockwise();
	for(std::size_t Index = 0; Index < Tiles.Size(); ++Index)
		Tiles.Update(Index, [](auto &Tile) { Tile.m_Angle = (Tile.m_Angle + 90 + 360) % 360; });
}

#endif
