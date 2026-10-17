/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "render_map.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/map.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

namespace
{
	class CEmptyTileCursor
	{
	};
	template<typename TSource>
	using CRenderRowCursor = std::conditional_t<std::is_pointer_v<TSource>, CEmptyTileCursor, CTileChunkCursor>;

	/** Keep row order and the native gameplay skip path; editor access is per span. */
	template<typename TSource, typename F>
	void VisitRenderRow(TSource &Source, int Width, int StartX, int EndX, int Y, bool Extend, CRenderRowCursor<TSource> &Cursor, F &&Render)
	{
		if constexpr(std::is_pointer_v<TSource>)
		{
			for(int X = StartX; X < EndX; ++X)
			{
				const int MapX = Extend ? std::clamp(X, 0, Width - 1) : X;
				X += Render(X, Source[MapX + static_cast<std::size_t>(Y) * Width]);
			}
		}
		else
		{
			std::array<typename TSource::CTileType, 32> aBuffer;
			for(int X = StartX; X < EndX;)
			{
				const int MapX = Extend ? std::clamp(X, 0, Width - 1) : X;
				const bool RepeatedEdge = MapX != X;
				const auto Row = Source.ReadRow(MapX, Y, RepeatedEdge ? MapX + 1 : std::min(EndX, Width), aBuffer, &Cursor);
				const int Length = RepeatedEdge ? (X < 0 ? std::min(EndX, 0) - X : EndX - X) : static_cast<int>(Row.m_Length);
				if(!Row.m_Cells.empty())
				{
					const auto *pCells = Row.m_Cells.data();
					if(RepeatedEdge)
					{
						if constexpr(std::is_same_v<typename TSource::CTileType, CTile>)
							if(pCells[0].m_Index == 0)
							{
								X += Length;
								continue;
							}
						for(int Offset = 0; Offset < Length; ++Offset)
							Render(X + Offset, pCells[0]);
					}
					else
						for(int Offset = 0; Offset < Length; ++Offset)
						{
							if constexpr(std::is_same_v<typename TSource::CTileType, CTile>)
								if(pCells[Offset].m_Index == 0)
									continue;
							Render(X + Offset, pCells[Offset]);
						}
				}
				X += Length;
			}
		}
	}
}

int IEnvelopePointAccess::FindPointIndex(CFixedTime Time) const
{
	// binary search for the interval around Time
	int Low = 0;
	int High = NumPoints() - 2;
	int FoundIndex = -1;

	while(Low <= High)
	{
		int Mid = Low + (High - Low) / 2;
		const CEnvPoint *pMid = GetPoint(Mid);
		const CEnvPoint *pNext = GetPoint(Mid + 1);
		if(Time >= pMid->m_Time && Time < pNext->m_Time)
		{
			FoundIndex = Mid;
			break;
		}
		else if(Time < pMid->m_Time)
		{
			High = Mid - 1;
		}
		else
		{
			Low = Mid + 1;
		}
	}
	return FoundIndex;
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(IMap *pMap)
{
	bool FoundBezierEnvelope = false;
	int EnvelopeStart, EnvelopeNum;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopeStart, &EnvelopeNum);
	for(int EnvelopeIndex = 0; EnvelopeIndex < EnvelopeNum; EnvelopeIndex++)
	{
		CMapItemEnvelope *pEnvelope = static_cast<CMapItemEnvelope *>(pMap->GetItem(EnvelopeStart + EnvelopeIndex));
		if(pEnvelope->m_Version >= CMapItemEnvelope::VERSION_TEEWORLDS_BEZIER)
		{
			FoundBezierEnvelope = true;
			break;
		}
	}

	if(FoundBezierEnvelope)
	{
		m_pPoints = nullptr;
		m_pPointsBezier = nullptr;

		int EnvPointStart, FakeEnvPointNum;
		pMap->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPointsBezierUpstream = static_cast<CEnvPointBezier_upstream *>(pMap->GetItem(EnvPointStart));
		else
			m_pPointsBezierUpstream = nullptr;

		m_NumPointsMax = pMap->GetItemSize(EnvPointStart) / sizeof(CEnvPointBezier_upstream);
	}
	else
	{
		int EnvPointStart, FakeEnvPointNum;
		pMap->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPoints = static_cast<CEnvPoint *>(pMap->GetItem(EnvPointStart));
		else
			m_pPoints = nullptr;

		m_NumPointsMax = pMap->GetItemSize(EnvPointStart) / sizeof(CEnvPoint);

		int EnvPointBezierStart, FakeEnvPointBezierNum;
		pMap->GetType(MAPITEMTYPE_ENVPOINTS_BEZIER, &EnvPointBezierStart, &FakeEnvPointBezierNum);
		const int NumPointsBezier = pMap->GetItemSize(EnvPointBezierStart) / sizeof(CEnvPointBezier);
		if(FakeEnvPointBezierNum > 0 && m_NumPointsMax == NumPointsBezier)
			m_pPointsBezier = static_cast<CEnvPointBezier *>(pMap->GetItem(EnvPointBezierStart));
		else
			m_pPointsBezier = nullptr;

		m_pPointsBezierUpstream = nullptr;
	}

	SetPointsRange(0, m_NumPointsMax);
}

void CMapBasedEnvelopePointAccess::SetPointsRange(int StartPoint, int NumPoints)
{
	m_StartPoint = std::clamp(StartPoint, 0, m_NumPointsMax);
	m_NumPoints = std::clamp(NumPoints, 0, m_NumPointsMax - m_StartPoint);
}

int CMapBasedEnvelopePointAccess::StartPoint() const
{
	return m_StartPoint;
}

int CMapBasedEnvelopePointAccess::NumPoints() const
{
	return m_NumPoints;
}

int CMapBasedEnvelopePointAccess::NumPointsMax() const
{
	return m_NumPointsMax;
}

const CEnvPoint *CMapBasedEnvelopePointAccess::GetPoint(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPoints != nullptr)
		return &m_pPoints[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint];
	return nullptr;
}

const CEnvPointBezier *CMapBasedEnvelopePointAccess::GetBezier(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPointsBezier != nullptr)
		return &m_pPointsBezier[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint].m_Bezier;
	return nullptr;
}

static float SolveBezier(float x, float p0, float p1, float p2, float p3)
{
	const double x3 = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
	const double x2 = 3.0 * p0 - 6.0 * p1 + 3.0 * p2;
	const double x1 = -3.0 * p0 + 3.0 * p1;
	const double x0 = p0 - x;

	if(x3 == 0.0 && x2 == 0.0)
	{
		// linear
		// a * t + b = 0
		const double a = x1;
		const double b = x0;

		if(a == 0.0)
			return 0.0f;
		return -b / a;
	}
	else if(x3 == 0.0)
	{
		// quadratic
		// t * t + b * t + c = 0
		const double b = x1 / x2;
		const double c = x0 / x2;

		if(c == 0.0)
			return 0.0f;

		const double D = b * b - 4.0 * c;
		const double SqrtD = std::sqrt(D);

		const double t = (-b + SqrtD) / 2.0;

		if(0.0 <= t && t <= 1.0001)
			return t;
		return (-b - SqrtD) / 2.0;
	}
	else
	{
		// cubic
		// t * t * t + a * t * t + b * t * t + c = 0
		const double a = x2 / x3;
		const double b = x1 / x3;
		const double c = x0 / x3;

		// substitute t = y - a / 3
		const double Substitute = a / 3.0;

		// depressed form x^3 + px + q = 0
		// cardano's method
		const double p = b / 3.0 - a * a / 9.0;
		const double q = (2.0 * a * a * a / 27.0 - a * b / 3.0 + c) / 2.0;

		const double D = q * q + p * p * p;

		if(D > 0.0)
		{
			// only one 'real' solution
			const double s = std::sqrt(D);
			return std::cbrt(s - q) - std::cbrt(s + q) - Substitute;
		}
		else if(D == 0.0)
		{
			// one single, one double solution or triple solution
			const double s = std::cbrt(-q);
			const double t = 2.0 * s - Substitute;

			if(0.0 <= t && t <= 1.0001)
				return t;
			return (-s - Substitute);
		}
		else
		{
			// Casus irreducibilis ... ,_,
			const double Phi = std::acos(-q / std::sqrt(-(p * p * p))) / 3.0;
			const double s = 2.0 * std::sqrt(-p);

			const double t1 = s * std::cos(Phi) - Substitute;

			if(0.0 <= t1 && t1 <= 1.0001)
				return t1;

			const double t2 = -s * std::cos(Phi + pi / 3.0) - Substitute;

			if(0.0 <= t2 && t2 <= 1.0001)
				return t2;
			return -s * std::cos(Phi - pi / 3.0) - Substitute;
		}
	}
}

void CRenderMap::Init(IGraphics *pGraphics, ITextRender *pTextRender)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
}

void CRenderMap::RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result, size_t Channels)
{
	const int NumPoints = pPoints->NumPoints();
	if(NumPoints == 0)
	{
		return;
	}

	if(NumPoints == 1)
	{
		const CEnvPoint *pFirstPoint = pPoints->GetPoint(0);
		for(size_t c = 0; c < Channels; c++)
		{
			Result[c] = fx2f(pFirstPoint->m_aValues[c]);
		}
		return;
	}

	const CEnvPoint *pLastPoint = pPoints->GetPoint(NumPoints - 1);
	const int64_t MaxPointTime = (int64_t)pLastPoint->m_Time.GetInternal() * std::chrono::nanoseconds(1ms).count();
	if(MaxPointTime > 0) // TODO: remove this check when implementing a IO check for maps(in this case broken envelopes)
		TimeNanos = std::chrono::nanoseconds(TimeNanos.count() % MaxPointTime);
	else
		TimeNanos = decltype(TimeNanos)::zero();

	const double TimeMillis = TimeNanos.count() / (double)std::chrono::nanoseconds(1ms).count();

	int FoundIndex = pPoints->FindPointIndex(CFixedTime(TimeMillis));
	if(FoundIndex == -1)
	{
		for(size_t c = 0; c < Channels; c++)
		{
			Result[c] = fx2f(pLastPoint->m_aValues[c]);
		}
		return;
	}

	const CEnvPoint *pCurrentPoint = pPoints->GetPoint(FoundIndex);
	const CEnvPoint *pNextPoint = pPoints->GetPoint(FoundIndex + 1);

	const CFixedTime Delta = pNextPoint->m_Time - pCurrentPoint->m_Time;
	if(Delta <= CFixedTime(0))
	{
		for(size_t c = 0; c < Channels; c++)
		{
			Result[c] = fx2f(pCurrentPoint->m_aValues[c]);
		}
		return;
	}

	float a = (float)(TimeMillis - pCurrentPoint->m_Time.GetInternal()) / Delta.GetInternal();

	switch(pCurrentPoint->m_Curvetype)
	{
	case CURVETYPE_STEP:
		a = 0.0f;
		break;

	case CURVETYPE_SLOW:
		a = a * a * a;
		break;

	case CURVETYPE_FAST:
		a = 1.0f - a;
		a = 1.0f - a * a * a;
		break;

	case CURVETYPE_SMOOTH:
		a = -2.0f * a * a * a + 3.0f * a * a; // second hermite basis
		break;

	case CURVETYPE_BEZIER:
	{
		const CEnvPointBezier *pCurrentPointBezier = pPoints->GetBezier(FoundIndex);
		const CEnvPointBezier *pNextPointBezier = pPoints->GetBezier(FoundIndex + 1);
		if(pCurrentPointBezier == nullptr || pNextPointBezier == nullptr)
			break; // fallback to linear
		for(size_t c = 0; c < Channels; c++)
		{
			// monotonic 2d cubic bezier curve
			const vec2 p0 = vec2(pCurrentPoint->m_Time.GetInternal(), fx2f(pCurrentPoint->m_aValues[c]));
			const vec2 p3 = vec2(pNextPoint->m_Time.GetInternal(), fx2f(pNextPoint->m_aValues[c]));

			const vec2 OutTang = vec2(pCurrentPointBezier->m_aOutTangentDeltaX[c].GetInternal(), fx2f(pCurrentPointBezier->m_aOutTangentDeltaY[c]));
			const vec2 InTang = vec2(pNextPointBezier->m_aInTangentDeltaX[c].GetInternal(), fx2f(pNextPointBezier->m_aInTangentDeltaY[c]));

			vec2 p1 = p0 + OutTang;
			vec2 p2 = p3 + InTang;

			// validate bezier curve
			p1.x = std::clamp(p1.x, p0.x, p3.x);
			p2.x = std::clamp(p2.x, p0.x, p3.x);

			// solve x(a) = time for a
			a = std::clamp(SolveBezier(TimeMillis, p0.x, p1.x, p2.x, p3.x), 0.0f, 1.0f);

			// value = y(t)
			Result[c] = bezier(p0.y, p1.y, p2.y, p3.y, a);
		}
		return;
	}

	case CURVETYPE_LINEAR: [[fallthrough]];
	default:
		break;
	}

	for(size_t c = 0; c < Channels; c++)
	{
		const float v0 = fx2f(pCurrentPoint->m_aValues[c]);
		const float v1 = fx2f(pNextPoint->m_aValues[c]);
		Result[c] = v0 + (v1 - v0) * a;
	}
}

static void Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * std::cos(Rotation) - y * std::sin(Rotation) + pCenter->x);
	pPoint->y = (int)(x * std::sin(Rotation) + y * std::cos(Rotation) + pCenter->y);
}

void CRenderMap::ForceRenderQuads(const CQuad *pQuads, int NumQuads, int RenderFlags, const IEnvelopeEval *pEnvEval, float Alpha)
{
	Graphics()->TrianglesBegin();
	for(int Index = 0; Index < NumQuads; ++Index)
		RenderQuad(pQuads[Index], RenderFlags, pEnvEval, Alpha);
	Graphics()->TrianglesEnd();
}

void CRenderMap::RenderQuad(const CQuad &Quad, int RenderFlags, const IEnvelopeEval *pEnvEval, float Alpha)
{
	const float Conv = 1 / 255.0f;
	ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	pEnvEval->EnvelopeEval(Quad.m_ColorEnvOffset, Quad.m_ColorEnv, Color, 4);

	if(Color.a <= 0.0f)
		return;

	bool Opaque = false;
	/* TODO: Analyze quadtexture
	if(a < 0.01f || (q->m_aColors[0].a < 0.01f && q->m_aColors[1].a < 0.01f && q->m_aColors[2].a < 0.01f && q->m_aColors[3].a < 0.01f))
		Opaque = true;
	*/
	if(Opaque && !(RenderFlags & LAYERRENDERFLAG_OPAQUE))
		return;
	if(!Opaque && !(RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
		return;

	Graphics()->QuadsSetSubsetFree(
		fx2f(Quad.m_aTexcoords[0].x), fx2f(Quad.m_aTexcoords[0].y),
		fx2f(Quad.m_aTexcoords[1].x), fx2f(Quad.m_aTexcoords[1].y),
		fx2f(Quad.m_aTexcoords[2].x), fx2f(Quad.m_aTexcoords[2].y),
		fx2f(Quad.m_aTexcoords[3].x), fx2f(Quad.m_aTexcoords[3].y));

	ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
	pEnvEval->EnvelopeEval(Quad.m_PosEnvOffset, Quad.m_PosEnv, Position, 3);
	const vec2 Offset = vec2(Position.r, Position.g);
	const float Rotation = Position.b / 180.0f * pi;

	Graphics()->SetColor4(
		ColorRGBA(Quad.m_aColors[0].r, Quad.m_aColors[0].g, Quad.m_aColors[0].b, Quad.m_aColors[0].a * Alpha).Multiply(Color).Multiply(Conv),
		ColorRGBA(Quad.m_aColors[1].r, Quad.m_aColors[1].g, Quad.m_aColors[1].b, Quad.m_aColors[1].a * Alpha).Multiply(Color).Multiply(Conv),
		ColorRGBA(Quad.m_aColors[3].r, Quad.m_aColors[3].g, Quad.m_aColors[3].b, Quad.m_aColors[3].a * Alpha).Multiply(Color).Multiply(Conv),
		ColorRGBA(Quad.m_aColors[2].r, Quad.m_aColors[2].g, Quad.m_aColors[2].b, Quad.m_aColors[2].a * Alpha).Multiply(Color).Multiply(Conv));

	const CPoint *pPoints = Quad.m_aPoints;

	CPoint aRotated[4];
	if(Rotation != 0.0f)
	{
		for(size_t p = 0; p < std::size(aRotated); ++p)
		{
			aRotated[p] = Quad.m_aPoints[p];
			Rotate(&Quad.m_aPoints[4], &aRotated[p], Rotation);
		}
		pPoints = aRotated;
	}

	IGraphics::CFreeformItem Freeform(
		fx2f(pPoints[0].x) + Offset.x, fx2f(pPoints[0].y) + Offset.y,
		fx2f(pPoints[1].x) + Offset.x, fx2f(pPoints[1].y) + Offset.y,
		fx2f(pPoints[2].x) + Offset.x, fx2f(pPoints[2].y) + Offset.y,
		fx2f(pPoints[3].x) + Offset.x, fx2f(pPoints[3].y) + Offset.y);
	Graphics()->QuadsDrawFreeform(&Freeform, 1);
}

void CRenderMap::RenderTileRectangle(int RectX, int RectY, int RectW, int RectH,
	unsigned char IndexIn, unsigned char IndexOut,
	float Scale, ColorRGBA Color, int RenderFlags)
{
	CScreenRect ScreenRect = Graphics()->GetScreen();

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / ScreenRect.Width() * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			unsigned char Index = (x >= RectX && x < RectX + RectW && y >= RectY && y < RectY + RectH) ? IndexIn : IndexOut;
			if(Index)
			{
				bool Render = false;
				if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT)
					Render = true;

				if(Render)
				{
					int tx = Index % 16;
					int ty = Index / 16;
					int Px0 = tx * (1024 / 16);
					int Py0 = ty * (1024 / 16);
					int Px1 = Px0 + (1024 / 16) - 1;
					int Py1 = Py0 + (1024 / 16) - 1;

					float x0 = Nudge + Px0 / TexSize + Frac;
					float y0 = Nudge + Py0 / TexSize + Frac;
					float x1 = Nudge + Px1 / TexSize - Frac;
					float y1 = Nudge + Py0 / TexSize + Frac;
					float x2 = Nudge + Px1 / TexSize - Frac;
					float y2 = Nudge + Py1 / TexSize - Frac;
					float x3 = Nudge + Px0 / TexSize + Frac;
					float y3 = Nudge + Py1 / TexSize - Frac;

					if(Graphics()->HasTextureArraysSupport())
					{
						x0 = 0;
						y0 = 0;
						x1 = x0 + 1;
						y1 = y0;
						x2 = x0 + 1;
						y2 = y0 + 1;
						x3 = x0;
						y3 = y0 + 1;
					}

					if(Graphics()->HasTextureArraysSupport())
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
					}
					else
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}
				}
			}
		}
	}

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderTile(int x, int y, unsigned char Index, float Scale, ColorRGBA Color)
{
	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();

	CScreenRect ScreenRect = Graphics()->GetScreen();

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / Scale;
	float FinalTileSize = Scale / ScreenRect.Width() * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	int tx = Index % 16;
	int ty = Index / 16;
	int Px0 = tx * (1024 / 16);
	int Py0 = ty * (1024 / 16);
	int Px1 = Px0 + (1024 / 16) - 1;
	int Py1 = Py0 + (1024 / 16) - 1;

	float x0 = Nudge + Px0 / TexSize + Frac;
	float y0 = Nudge + Py0 / TexSize + Frac;
	float x1 = Nudge + Px1 / TexSize - Frac;
	float y1 = Nudge + Py0 / TexSize + Frac;
	float x2 = Nudge + Px1 / TexSize - Frac;
	float y2 = Nudge + Py1 / TexSize - Frac;
	float x3 = Nudge + Px0 / TexSize + Frac;
	float y3 = Nudge + Py1 / TexSize - Frac;

	if(Graphics()->HasTextureArraysSupport())
	{
		x0 = 0;
		y0 = 0;
		x1 = x0 + 1;
		y1 = y0;
		x2 = x0 + 1;
		y2 = y0 + 1;
		x3 = x0;
		y3 = y0 + 1;
	}

	if(Graphics()->HasTextureArraysSupport())
	{
		Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
		IGraphics::CQuadItem QuadItem(x, y, Scale, Scale);
		Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
	}
	else
	{
		Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
		IGraphics::CQuadItem QuadItem(x, y, Scale, Scale);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderTilemap(const CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	RenderTilemapImpl(pTiles, w, h, Scale, Color, RenderFlags);
}

void CRenderMap::RenderTilemap(CRenderTileSource<CTile> pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	RenderTilemapImpl(pTiles, w, h, Scale, Color, RenderFlags);
}

template<typename TSource>
void CRenderMap::RenderTilemapImpl(TSource pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	if(w <= 0 || h <= 0)
		return;

	CScreenRect ScreenRect = Graphics()->GetScreen();
	const bool TextureArrays = Graphics()->HasTextureArraysSupport();

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / ScreenRect.Width() * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	if(TextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	const bool ColorOpaque = Color.a > 254.0f / 255.0f;

	const bool ExtendTiles = (RenderFlags & TILERENDERFLAG_EXTEND) != 0;

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(!ExtendTiles)
	{
		StartY = std::max(0, StartY);
		StartX = std::max(0, StartX);
		EndY = std::min(h, EndY);
		EndX = std::min(w, EndX);
	}

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	// A translucent color cannot contribute to a pass without transparency.
	// Keep begin/color/end state changes even when no cells can be submitted.
	if(ColorOpaque || (RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
	{
		CRenderRowCursor<TSource> Cursor;
		for(int y = StartY; y < EndY; y++)
		{
			VisitRenderRow(pTiles, w, StartX, EndX, ExtendTiles ? std::clamp(y, 0, h - 1) : y, ExtendTiles, Cursor, [&](int x, const auto &Tile) {
				unsigned char Index = Tile.m_Index;
				if(Index)
				{
					unsigned char Flags = Tile.m_Flags;

					bool Render = false;
					if(ColorOpaque && Flags & TILEFLAG_OPAQUE)
					{
						if(RenderFlags & LAYERRENDERFLAG_OPAQUE)
							Render = true;
					}
					else
					{
						if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT)
							Render = true;
					}

					if(Render)
					{
						float x0 = 0, y0 = 0, x1 = 1, y1 = 0;
						float x2 = 1, y2 = 1, x3 = 0, y3 = 1;
						if(!TextureArrays)
						{
							int tx = Index % 16;
							int ty = Index / 16;
							int Px0 = tx * (1024 / 16);
							int Py0 = ty * (1024 / 16);
							int Px1 = Px0 + (1024 / 16) - 1;
							int Py1 = Py0 + (1024 / 16) - 1;

							x0 = Nudge + Px0 / TexSize + Frac;
							y0 = Nudge + Py0 / TexSize + Frac;
							x1 = Nudge + Px1 / TexSize - Frac;
							y1 = Nudge + Py0 / TexSize + Frac;
							x2 = Nudge + Px1 / TexSize - Frac;
							y2 = Nudge + Py1 / TexSize - Frac;
							x3 = Nudge + Px0 / TexSize + Frac;
							y3 = Nudge + Py1 / TexSize - Frac;
						}

						if(Flags & TILEFLAG_XFLIP)
						{
							x0 = x2;
							x1 = x3;
							x2 = x3;
							x3 = x0;
						}

						if(Flags & TILEFLAG_YFLIP)
						{
							y0 = y3;
							y2 = y1;
							y3 = y1;
							y1 = y0;
						}

						if(Flags & TILEFLAG_ROTATE)
						{
							float Tmp = x0;
							x0 = x3;
							x3 = x2;
							x2 = x1;
							x1 = Tmp;
							Tmp = y0;
							y0 = y3;
							y3 = y2;
							y2 = y1;
							y1 = Tmp;
						}

						if(TextureArrays)
						{
							Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
							IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
							Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
						}
						else
						{
							Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
							IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
							Graphics()->QuadsDrawTL(&QuadItem, 1);
						}
					}
				}
				return Tile.m_Skip;
			});
		}
	}

	if(TextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderTeleOverlay(const CTeleTile *pTele, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderTeleOverlayImpl(pTele, w, h, Scale, OverlayRenderFlag, Alpha);
}

void CRenderMap::RenderTeleOverlay(CRenderTileSource<CTeleTile> pTele, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderTeleOverlayImpl(pTele, w, h, Scale, OverlayRenderFlag, Alpha);
}

template<typename TSource>
void CRenderMap::RenderTeleOverlayImpl(TSource pTele, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	if(!(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT))
		return;

	CScreenRect ScreenRect = Graphics()->GetScreen();

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	StartY = std::max(0, StartY);
	StartX = std::max(0, StartX);
	EndY = std::min(h, EndY);
	EndX = std::min(w, EndX);

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	CRenderRowCursor<TSource> Cursor;
	for(int y = StartY; y < EndY; y++)
	{
		VisitRenderRow(pTele, w, StartX, EndX, y, false, Cursor, [&](int x, const auto &Tile) {
			unsigned char Index = Tile.m_Number;
			if(Index && IsTeleTileNumberUsedAny(Tile.m_Type))
			{
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				// Auto-resize text to fit inside the tile
				float ScaledWidth = TextRender()->TextWidth(Size * Scale, aBuf, -1);
				float Factor = std::clamp(Scale / ScaledWidth, 0.0f, 1.0f);
				float LocalSize = Size * Factor;
				float ToCenterOffset = (1 - LocalSize) / 2.f;
				TextRender()->Text((x + 0.5f) * Scale - (ScaledWidth * Factor) / 2.0f, (y + ToCenterOffset) * Scale, LocalSize * Scale, aBuf);
			}
			return 0;
		});
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderSpeedupOverlay(const CSpeedupTile *pSpeedup, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderSpeedupOverlayImpl(pSpeedup, w, h, Scale, OverlayRenderFlag, Alpha);
}

void CRenderMap::RenderSpeedupOverlay(CRenderTileSource<CSpeedupTile> pSpeedup, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderSpeedupOverlayImpl(pSpeedup, w, h, Scale, OverlayRenderFlag, Alpha);
}

template<typename TSource>
void CRenderMap::RenderSpeedupOverlayImpl(TSource pSpeedup, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	CScreenRect ScreenRect = Graphics()->GetScreen();

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	StartY = std::max(0, StartY);
	StartX = std::max(0, StartX);
	EndY = std::min(h, EndY);
	EndX = std::min(w, EndX);

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	CRenderRowCursor<TSource> Cursor;
	for(int y = StartY; y < EndY; y++)
	{
		VisitRenderRow(pSpeedup, w, StartX, EndX, y, false, Cursor, [&](int x, const auto &Tile) {
			int Force = (int)Tile.m_Force;
			int MaxSpeed = (int)Tile.m_MaxSpeed;
			int Type = (int)Tile.m_Type;
			int Angle = (int)Tile.m_Angle;
			if((Force && Type == TILE_SPEED_BOOST_OLD) || ((Force || MaxSpeed) && Type == TILE_SPEED_BOOST) || (OverlayRenderFlag & OVERLAYRENDERFLAG_EDITOR && (Type || Force || MaxSpeed || Angle)))
			{
				if(IsValidSpeedupTile(Type))
				{
					// draw arrow
					Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_Id);
					Graphics()->QuadsBegin();
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					Graphics()->SelectSprite(SPRITE_SPEEDUP_ARROW);
					Graphics()->QuadsSetRotation(Tile.m_Angle * (pi / 180.0f));
					Graphics()->DrawSprite(x * Scale + 16, y * Scale + 16, 35.0f);
					Graphics()->QuadsEnd();

					// draw force and max speed
					if(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT)
					{
						str_format(aBuf, sizeof(aBuf), "%d", Force);
						TextRender()->Text(x * Scale, (y + 0.5f + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
						if(MaxSpeed)
						{
							str_format(aBuf, sizeof(aBuf), "%d", MaxSpeed);
							TextRender()->Text(x * Scale, (y + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
						}
					}
				}
				else
				{
					// draw all three values
					if(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT)
					{
						float LineSpacing = Size * Scale / 3.f;
						float BaseY = (y + ToCenterOffset) * Scale;
						str_format(aBuf, sizeof(aBuf), "%d", Force);
						TextRender()->Text(x * Scale, BaseY, LineSpacing, aBuf);
						str_format(aBuf, sizeof(aBuf), "%d", MaxSpeed);
						TextRender()->Text(x * Scale, BaseY + LineSpacing, LineSpacing, aBuf);
						str_format(aBuf, sizeof(aBuf), "%d", Angle);
						TextRender()->Text(x * Scale, BaseY + 2 * LineSpacing, LineSpacing, aBuf);
					}
				}
			}
			return 0;
		});
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderSwitchOverlay(const CSwitchTile *pSwitch, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderSwitchOverlayImpl(pSwitch, w, h, Scale, OverlayRenderFlag, Alpha);
}

void CRenderMap::RenderSwitchOverlay(CRenderTileSource<CSwitchTile> pSwitch, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderSwitchOverlayImpl(pSwitch, w, h, Scale, OverlayRenderFlag, Alpha);
}

template<typename TSource>
void CRenderMap::RenderSwitchOverlayImpl(TSource pSwitch, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	if(!(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT))
		return;

	CScreenRect ScreenRect = Graphics()->GetScreen();

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	StartY = std::max(0, StartY);
	StartX = std::max(0, StartX);
	EndY = std::min(h, EndY);
	EndX = std::min(w, EndX);

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	CRenderRowCursor<TSource> Cursor;
	for(int y = StartY; y < EndY; y++)
	{
		VisitRenderRow(pSwitch, w, StartX, EndX, y, false, Cursor, [&](int x, const auto &Tile) {
			unsigned char Index = Tile.m_Number;
			if(Index && IsSwitchTileNumberUsed(Tile.m_Type))
			{
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				TextRender()->Text(x * Scale, (y + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
			}

			unsigned char Delay = Tile.m_Delay;
			if(Delay && IsSwitchTileDelayUsed(Tile.m_Type))
			{
				str_format(aBuf, sizeof(aBuf), "%d", Delay);
				TextRender()->Text(x * Scale, (y + 0.5f + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
			}
			return 0;
		});
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderTuneOverlay(const CTuneTile *pTune, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderTuneOverlayImpl(pTune, w, h, Scale, OverlayRenderFlag, Alpha);
}

void CRenderMap::RenderTuneOverlay(CRenderTileSource<CTuneTile> pTune, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	RenderTuneOverlayImpl(pTune, w, h, Scale, OverlayRenderFlag, Alpha);
}

template<typename TSource>
void CRenderMap::RenderTuneOverlayImpl(TSource pTune, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	if(!(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT))
		return;

	CScreenRect ScreenRect = Graphics()->GetScreen();

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	StartY = std::max(0, StartY);
	StartX = std::max(0, StartX);
	EndY = std::min(h, EndY);
	EndX = std::min(w, EndX);

	float Size = g_Config.m_ClTextEntitiesSize / 200.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	CRenderRowCursor<TSource> Cursor;
	for(int y = StartY; y < EndY; y++)
	{
		VisitRenderRow(pTune, w, StartX, EndX, y, false, Cursor, [&](int x, const auto &Tile) {
			unsigned char Index = Tile.m_Number;
			if(Index)
			{
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				// Auto-resize text to fit inside the tile
				float ScaledWidth = TextRender()->TextWidth(Size * Scale, aBuf, -1);
				float Factor = std::clamp(Scale / ScaledWidth, 0.0f, 1.0f);
				float LocalSize = Size * Factor;
				float ToCenterOffset = (1 - LocalSize) / 2.f;
				TextRender()->Text((x + 0.5f) * Scale - (ScaledWidth * Factor) / 2.0f, (y + ToCenterOffset) * Scale, LocalSize * Scale, aBuf);
			}
			return 0;
		});
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	CScreenRect ScreenRect = Graphics()->GetScreen();

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / ScreenRect.Width() * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	bool ExtendTiles = (RenderFlags & TILERENDERFLAG_EXTEND) != 0;

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(!ExtendTiles)
	{
		StartY = std::max(0, StartY);
		StartX = std::max(0, StartX);
		EndY = std::min(h, EndY);
		EndX = std::min(w, EndX);
	}

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(ExtendTiles)
			{
				if(mx < 0)
					mx = 0;
				if(mx >= w)
					mx = w - 1;
				if(my < 0)
					my = 0;
				if(my >= h)
					my = h - 1;
			}

			const std::size_t c = mx + static_cast<std::size_t>(my) * w;

			unsigned char Index = pTele[c].m_Type;
			if(Index)
			{
				bool Render = false;
				if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT)
					Render = true;

				if(Render)
				{
					int tx = Index % 16;
					int ty = Index / 16;
					int Px0 = tx * (1024 / 16);
					int Py0 = ty * (1024 / 16);
					int Px1 = Px0 + (1024 / 16) - 1;
					int Py1 = Py0 + (1024 / 16) - 1;

					float x0 = Nudge + Px0 / TexSize + Frac;
					float y0 = Nudge + Py0 / TexSize + Frac;
					float x1 = Nudge + Px1 / TexSize - Frac;
					float y1 = Nudge + Py0 / TexSize + Frac;
					float x2 = Nudge + Px1 / TexSize - Frac;
					float y2 = Nudge + Py1 / TexSize - Frac;
					float x3 = Nudge + Px0 / TexSize + Frac;
					float y3 = Nudge + Py1 / TexSize - Frac;

					if(Graphics()->HasTextureArraysSupport())
					{
						x0 = 0;
						y0 = 0;
						x1 = x0 + 1;
						y1 = y0;
						x2 = x0 + 1;
						y2 = y0 + 1;
						x3 = x0;
						y3 = y0 + 1;
					}

					if(Graphics()->HasTextureArraysSupport())
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
					}
					else
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}
				}
			}
		}

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderSwitchmap(CSwitchTile *pSwitchTile, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	CScreenRect ScreenRect = Graphics()->GetScreen();

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / ScreenRect.Width() * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	bool ExtendTiles = (RenderFlags & TILERENDERFLAG_EXTEND) != 0;

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(!ExtendTiles)
	{
		StartY = std::max(0, StartY);
		StartX = std::max(0, StartX);
		EndY = std::min(h, EndY);
		EndX = std::min(w, EndX);
	}

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(ExtendTiles)
			{
				if(mx < 0)
					mx = 0;
				if(mx >= w)
					mx = w - 1;
				if(my < 0)
					my = 0;
				if(my >= h)
					my = h - 1;
			}

			const std::size_t c = mx + static_cast<std::size_t>(my) * w;

			unsigned char Index = pSwitchTile[c].m_Type;
			if(Index)
			{
				if(Index == TILE_SWITCHTIMEDOPEN)
					Index = 8;

				unsigned char Flags = pSwitchTile[c].m_Flags;

				bool Render = false;
				if(Flags & TILEFLAG_OPAQUE)
				{
					if(RenderFlags & LAYERRENDERFLAG_OPAQUE)
						Render = true;
				}
				else
				{
					if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT)
						Render = true;
				}

				if(Render)
				{
					int tx = Index % 16;
					int ty = Index / 16;
					int Px0 = tx * (1024 / 16);
					int Py0 = ty * (1024 / 16);
					int Px1 = Px0 + (1024 / 16) - 1;
					int Py1 = Py0 + (1024 / 16) - 1;

					float x0 = Nudge + Px0 / TexSize + Frac;
					float y0 = Nudge + Py0 / TexSize + Frac;
					float x1 = Nudge + Px1 / TexSize - Frac;
					float y1 = Nudge + Py0 / TexSize + Frac;
					float x2 = Nudge + Px1 / TexSize - Frac;
					float y2 = Nudge + Py1 / TexSize - Frac;
					float x3 = Nudge + Px0 / TexSize + Frac;
					float y3 = Nudge + Py1 / TexSize - Frac;

					if(Graphics()->HasTextureArraysSupport())
					{
						x0 = 0;
						y0 = 0;
						x1 = x0 + 1;
						y1 = y0;
						x2 = x0 + 1;
						y2 = y0 + 1;
						x3 = x0;
						y3 = y0 + 1;
					}

					if(Flags & TILEFLAG_XFLIP)
					{
						x0 = x2;
						x1 = x3;
						x2 = x3;
						x3 = x0;
					}

					if(Flags & TILEFLAG_YFLIP)
					{
						y0 = y3;
						y2 = y1;
						y3 = y1;
						y1 = y0;
					}

					if(Flags & TILEFLAG_ROTATE)
					{
						float Tmp = x0;
						x0 = x3;
						x3 = x2;
						x2 = x1;
						x1 = Tmp;
						Tmp = y0;
						y0 = y3;
						y3 = y2;
						y2 = y1;
						y1 = Tmp;
					}

					if(Graphics()->HasTextureArraysSupport())
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
					}
					else
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}
				}
			}
		}

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags, CTuneColorMapper *pTuneColorMapper)
{
	CScreenRect ScreenRect = Graphics()->GetScreen();

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / ScreenRect.Width() * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	bool ExtendTiles = (RenderFlags & TILERENDERFLAG_EXTEND) != 0;

	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	if(!ExtendTiles)
	{
		StartY = std::max(0, StartY);
		StartX = std::max(0, StartX);
		EndY = std::min(h, EndY);
		EndX = std::min(w, EndX);
	}

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(ExtendTiles)
			{
				if(mx < 0)
					mx = 0;
				if(mx >= w)
					mx = w - 1;
				if(my < 0)
					my = 0;
				if(my >= h)
					my = h - 1;
			}

			const std::size_t c = mx + static_cast<std::size_t>(my) * w;

			const unsigned char Index = pTune[c].m_Type;

			if(Index)
			{
				bool Render = false;
				if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT)
					Render = true;

				if(Render)
				{
					const unsigned char Number = pTune[c].m_Number;

					if(Number == 0 || pTuneColorMapper == nullptr)
						Graphics()->SetColor(Color);
					else
					{
						uint8_t ColorIndex = pTuneColorMapper->TuneNumberToColorIndex(Number);
						Graphics()->SetColor(pTuneColorMapper->TuneColorIndexToColor(ColorIndex).Multiply(Color));
					}

					int tx = Index % 16;
					int ty = Index / 16;
					int Px0 = tx * (1024 / 16);
					int Py0 = ty * (1024 / 16);
					int Px1 = Px0 + (1024 / 16) - 1;
					int Py1 = Py0 + (1024 / 16) - 1;

					float x0 = Nudge + Px0 / TexSize + Frac;
					float y0 = Nudge + Py0 / TexSize + Frac;
					float x1 = Nudge + Px1 / TexSize - Frac;
					float y1 = Nudge + Py0 / TexSize + Frac;
					float x2 = Nudge + Px1 / TexSize - Frac;
					float y2 = Nudge + Py1 / TexSize - Frac;
					float x3 = Nudge + Px0 / TexSize + Frac;
					float y3 = Nudge + Py1 / TexSize - Frac;

					if(Graphics()->HasTextureArraysSupport())
					{
						x0 = 0;
						y0 = 0;
						x1 = x0 + 1;
						y1 = y0;
						x2 = x0 + 1;
						y2 = y0 + 1;
						x3 = x0;
						y3 = y0 + 1;
					}

					if(Graphics()->HasTextureArraysSupport())
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
					}
					else
					{
						Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
						IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}
				}
			}
		}

	if(Graphics()->HasTextureArraysSupport())
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenRect);
}

void CRenderMap::RenderDebugClip(float ClipX, float ClipY, float ClipW, float ClipH, ColorRGBA Color, float Zoom, const char *pLabel)
{
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color);
	IGraphics::CLineItem aLineItems[] = {
		IGraphics::CLineItem(ClipX, ClipY, ClipX, ClipY + ClipH),
		IGraphics::CLineItem(ClipX + ClipW, ClipY, ClipX + ClipW, ClipY + ClipH),
		IGraphics::CLineItem(ClipX, ClipY, ClipX + ClipW, ClipY),
		IGraphics::CLineItem(ClipX, ClipY + ClipH, ClipX + ClipW, ClipY + ClipH),
	};
	Graphics()->LinesDraw(aLineItems, std::size(aLineItems));
	Graphics()->LinesEnd();

	TextRender()->TextColor(Color);

	// clamp zoom and set line width, because otherwise the text can be partially clipped out
	TextRender()->Text(ClipX, ClipY, std::min(12.0f * Zoom, 20.0f), pLabel, ClipW);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

CTuneColorMapper::CTuneColorMapper()
{
	Reset();
}

uint8_t CTuneColorMapper::TuneNumberToColorIndex(uint8_t TuneNumber)
{
	if(TuneNumber == 0)
		return 0;

	uint8_t &TuneColorIndex = m_aTuneNumberToColorIndex[TuneNumber - 1];
	if(TuneColorIndex == 0)
	{
		TuneColorIndex = m_NextTuneNumberIndex + 1;
		++m_NextTuneNumberIndex;
	}
	return TuneColorIndex;
}

ColorRGBA CTuneColorMapper::TuneColorIndexToColor(uint8_t TuneColorIndex) const
{
	if(TuneColorIndex == 0)
		return ColorRGBA(1.0f, 1.0f, 1.0f);

	float Hue = std::fmod((TuneColorIndex - 1) * normalized_golden_angle, 1.0f);
	return color_cast<ColorRGBA>(ColorHSLA(Hue, 0.75f, 0.5f, 1.0f));
}

void CTuneColorMapper::Reset()
{
	m_aTuneNumberToColorIndex.fill(0);
	m_NextTuneNumberIndex = 0;
}
