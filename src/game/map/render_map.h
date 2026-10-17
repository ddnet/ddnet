#ifndef GAME_MAP_RENDER_MAP_H
#define GAME_MAP_RENDER_MAP_H

#include <base/color.h>

#include <game/map/render_interfaces.h>
#include <game/map/tile_cursor.h>
#include <game/mapitems.h>

#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

enum
{
	LAYERRENDERFLAG_OPAQUE = 1,
	LAYERRENDERFLAG_TRANSPARENT = 2,

	TILERENDERFLAG_EXTEND = 4,

	OVERLAYRENDERFLAG_TEXT = 1,
	OVERLAYRENDERFLAG_EDITOR = 2,
};

class IEnvelopePointAccess
{
public:
	virtual ~IEnvelopePointAccess() = default;
	virtual int NumPoints() const = 0;
	virtual const CEnvPoint *GetPoint(int Index) const = 0;
	virtual const CEnvPointBezier *GetBezier(int Index) const = 0;
	int FindPointIndex(CFixedTime Time) const;
};

class CMapBasedEnvelopePointAccess : public IEnvelopePointAccess
{
	int m_StartPoint;
	int m_NumPoints;
	int m_NumPointsMax;
	CEnvPoint *m_pPoints;
	CEnvPointBezier *m_pPointsBezier;
	CEnvPointBezier_upstream *m_pPointsBezierUpstream;

public:
	CMapBasedEnvelopePointAccess(class IMap *pMap);
	void SetPointsRange(int StartPoint, int NumPoints);
	int StartPoint() const;
	int NumPoints() const override;
	int NumPointsMax() const;
	const CEnvPoint *GetPoint(int Index) const override;
	const CEnvPointBezier *GetBezier(int Index) const override;
};

class IGraphics;
class ITextRender;

class CTuneColorMapper
{
public:
	CTuneColorMapper();
	uint8_t TuneNumberToColorIndex(uint8_t TuneNumber);
	ColorRGBA TuneColorIndexToColor(uint8_t TuneColorIndex) const;
	void Reset();

private:
	std::array<uint8_t, 255> m_aTuneNumberToColorIndex;
	uint8_t m_NextTuneNumberIndex = 0;
};

/** Borrowed read-only editor tiles. Gameplay keeps the contiguous pointer overload. */
template<typename T>
class CRenderTileSource
{
	const void *m_pSource;
	std::size_t m_Width;
	const unsigned char *m_pFlags = nullptr;

public:
	class CRowSpan
	{
	public:
		std::span<const T> m_Cells;
		std::size_t m_Length;
	};

private:
	CRowSpan (*m_ReadRow)(const void *, std::size_t, std::size_t, std::size_t, std::span<T>, const unsigned char *, CTileChunkCursor *);

public:
	template<typename P>
	explicit CRenderTileSource(const P &Plane, const unsigned char *pFlags = nullptr) :
		m_pSource(&Plane),
		m_Width(Plane.Width()),
		m_pFlags(pFlags),
		m_ReadRow([](const void *pSource, std::size_t X, std::size_t Y, std::size_t EndX, std::span<T> Buffer, const unsigned char *pDerivedFlags, CTileChunkCursor *pCursor) -> CRowSpan {
			const auto *pPlane = static_cast<const P *>(pSource);
			const auto Row = pCursor ? pPlane->ReadRowCached(X, Y, EndX, *pCursor) : pPlane->ReadRow(X, Y, EndX);
			if(Row.m_Cells.empty())
				return {{}, Row.m_Length};
			const auto Length = std::min(Row.m_Length, Buffer.size());
			const auto *pCells = Row.m_Cells.data();
			auto *pOutput = Buffer.data();
			// Bounds were checked once for this span. Avoid debug iterator and
			// conversion calls per visible cell, including the many empty cells.
			if constexpr(std::is_same_v<T, CTile>)
			{
				static constexpr unsigned char s_aNoOpacity[256]{};
				const auto *pOpacity = pDerivedFlags ? pDerivedFlags : s_aNoOpacity;
				for(std::size_t Index = 0; Index < Length; ++Index)
				{
					const auto &Cell = pCells[Index];
					pOutput[Index] = {Cell.m_Index, static_cast<unsigned char>(Cell.m_Flags | (pOpacity[Cell.m_Index] & TILEFLAG_OPAQUE)), 0, 0};
				}
			}
			else
				for(std::size_t Index = 0; Index < Length; ++Index)
					pOutput[Index] = static_cast<T>(pCells[Index]);
			return {std::span<const T>(pOutput, Length), Length};
		})
	{
	}
	/** The scratch buffer and borrowed rows belong to this render invocation. */
	CRowSpan ReadRow(std::size_t X, std::size_t Y, std::size_t EndX, std::span<T> Buffer, CTileChunkCursor *pCursor = nullptr) const
	{
		return m_ReadRow(m_pSource, X, Y, EndX, Buffer, m_pFlags, pCursor);
	}
	T operator[](std::size_t Index) const
	{
		T Value{};
		m_ReadRow(m_pSource, Index % m_Width, Index / m_Width, Index % m_Width + 1, std::span<T>(&Value, 1), m_pFlags, nullptr);
		if constexpr(std::is_same_v<T, CTile>)
			if(m_pFlags)
				Value.m_Flags |= m_pFlags[Value.m_Index] & TILEFLAG_OPAQUE;
		return Value;
	}
	using CTileType = T;
};

class CRenderMap
{
	template<typename TSource>
	void RenderTilemapImpl(TSource pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	template<typename TSource>
	void RenderTeleOverlayImpl(TSource pTele, int w, int h, float Scale, int OverlayRenderFlags, float Alpha);
	template<typename TSource>
	void RenderSpeedupOverlayImpl(TSource pSpeedup, int w, int h, float Scale, int OverlayRenderFlags, float Alpha);
	template<typename TSource>
	void RenderSwitchOverlayImpl(TSource pSwitch, int w, int h, float Scale, int OverlayRenderFlags, float Alpha);
	template<typename TSource>
	void RenderTuneOverlayImpl(TSource pTune, int w, int h, float Scale, int OverlayRenderFlags, float Alpha);
	IGraphics *m_pGraphics;
	ITextRender *m_pTextRender;

public:
	void Init(IGraphics *pGraphics, ITextRender *pTextRender);
	IGraphics *Graphics() { return m_pGraphics; }
	ITextRender *TextRender() { return m_pTextRender; }

	// map render methods (render_map.cpp)
	static void RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result, size_t Channels);
	void RenderQuad(const CQuad &Quad, int Flags, const IEnvelopeEval *pEnvEval, float Alpha);
	void ForceRenderQuads(const CQuad *pQuads, int NumQuads, int Flags, const IEnvelopeEval *pEnvEval, float Alpha = 1.0f);
	void RenderTile(int x, int y, unsigned char Index, float Scale, ColorRGBA Color);
	void RenderTilemap(const CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderTilemap(CRenderTileSource<CTile> Tiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);

	// render a rectangle made of IndexIn tiles, over a background made of IndexOut tiles
	// the rectangle include all tiles in [RectX, RectX+RectW-1] x [RectY, RectY+RectH-1]
	void RenderTileRectangle(int RectX, int RectY, int RectW, int RectH, unsigned char IndexIn, unsigned char IndexOut, float Scale, ColorRGBA Color, int RenderFlags);

	// DDRace
	void RenderTeleOverlay(const CTeleTile *pTele, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f);
	void RenderTeleOverlay(CRenderTileSource<CTeleTile> pTele, int w, int h, float Scale, int OverlayRenderFlag, float Alpha = 1.0f);
	void RenderSpeedupOverlay(const CSpeedupTile *pSpeedup, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f);
	void RenderSpeedupOverlay(CRenderTileSource<CSpeedupTile> pSpeedup, int w, int h, float Scale, int OverlayRenderFlag, float Alpha = 1.0f);
	void RenderSwitchOverlay(const CSwitchTile *pSwitch, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f);
	void RenderSwitchOverlay(CRenderTileSource<CSwitchTile> pSwitch, int w, int h, float Scale, int OverlayRenderFlag, float Alpha = 1.0f);
	void RenderTuneOverlay(const CTuneTile *pTune, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f);
	void RenderTuneOverlay(CRenderTileSource<CTuneTile> pTune, int w, int h, float Scale, int OverlayRenderFlag, float Alpha = 1.0f);
	void RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderSwitchmap(CSwitchTile *pSwitch, int w, int h, float Scale, ColorRGBA Color, int RenderFlags);
	void RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags, CTuneColorMapper *pTuneColorMapper);

	void RenderDebugClip(float ClipX, float ClipY, float ClipW, float ClipH, ColorRGBA Color, float Zoom, const char *pLabel);
};

#endif
