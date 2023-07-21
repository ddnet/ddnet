/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/textrender.h>

#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/map.h>

#include "render.h"

#include <game/generated/client_data.h>

#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(CDataFileReader *pReader)
{
	bool FoundBezierEnvelope = false;
	int EnvStart, EnvNum;
	pReader->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	for(int EnvIndex = 0; EnvIndex < EnvNum; EnvIndex++)
	{
		CMapItemEnvelope *pEnvelope = static_cast<CMapItemEnvelope *>(pReader->GetItem(EnvStart + EnvIndex));
		if(pEnvelope->m_Version >= CMapItemEnvelope_v3::CURRENT_VERSION)
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
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPointsBezierUpstream = static_cast<CEnvPointBezier_upstream *>(pReader->GetItem(EnvPointStart));
		else
			m_pPointsBezierUpstream = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPointBezier_upstream);
	}
	else
	{
		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPoints = static_cast<CEnvPoint *>(pReader->GetItem(EnvPointStart));
		else
			m_pPoints = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPoint);

		int EnvPointBezierStart, FakeEnvPointBezierNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS_BEZIER, &EnvPointBezierStart, &FakeEnvPointBezierNum);
		const int NumPointsBezier = pReader->GetItemSize(EnvPointBezierStart) / sizeof(CEnvPointBezier);
		if(FakeEnvPointBezierNum > 0 && m_NumPointsMax == NumPointsBezier)
			m_pPointsBezier = static_cast<CEnvPointBezier *>(pReader->GetItem(EnvPointBezierStart));
		else
			m_pPointsBezier = nullptr;

		m_pPointsBezierUpstream = nullptr;
	}

	SetPointsRange(0, m_NumPointsMax);
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(IMap *pMap) :
	CMapBasedEnvelopePointAccess(static_cast<CMap *>(pMap)->GetReader())
{
}

void CMapBasedEnvelopePointAccess::SetPointsRange(int StartPoint, int NumPoints)
{
	m_StartPoint = clamp(StartPoint, 0, m_NumPointsMax);
	m_NumPoints = clamp(NumPoints, 0, maximum(m_NumPointsMax - StartPoint, 0));
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

static void ValidateFCurve(const vec2 &p0, vec2 &p1, vec2 &p2, const vec2 &p3)
{
	// validate the bezier curve
	p1.x = clamp(p1.x, p0.x, p3.x);
	p2.x = clamp(p2.x, p0.x, p3.x);
}

static double CubicRoot(double x)
{
	if(x == 0.0)
		return 0.0;
	else if(x < 0.0)
		return -std::exp(std::log(-x) / 3.0);
	else
		return std::exp(std::log(x) / 3.0);
}

static float SolveBezier(float x, float p0, float p1, float p2, float p3)
{
	// check for valid f-curve
	// we only take care of monotonic bezier curves, so there has to be exactly 1 real solution
	if(!(p0 <= x && x <= p3) || !(p0 <= p1 && p1 <= p3) || !(p0 <= p2 && p2 <= p3))
		return 0.0f;

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
		// t * t + b * t +c = 0
		const double b = x1 / x2;
		const double c = x0 / x2;

		if(c == 0.0)
			return 0.0f;

		const double D = b * b - 4.0 * c;
		const double SqrtD = std::sqrt(D);

		const double t = (-b + SqrtD) / 2.0;

		if(0.0 <= t && t <= 1.0001f)
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
		const double sub = a / 3.0;

		// depressed form x^3 + px + q = 0
		// cardano's method
		const double p = b / 3.0 - a * a / 9.0;
		const double q = (2.0 * a * a * a / 27.0 - a * b / 3.0 + c) / 2.0;

		const double D = q * q + p * p * p;

		if(D > 0.0)
		{
			// only one 'real' solution
			const double s = std::sqrt(D);
			return CubicRoot(s - q) - CubicRoot(s + q) - sub;
		}
		else if(D == 0.0)
		{
			// one single, one double solution or triple solution
			const double s = CubicRoot(-q);
			const double t = 2.0 * s - sub;

			if(0.0 <= t && t <= 1.0001f)
				return t;
			return (-s - sub);
		}
		else
		{
			// Casus irreductibilis ... ,_,
			const double phi = std::acos(-q / std::sqrt(-(p * p * p))) / 3.0;
			const double s = 2.0 * std::sqrt(-p);

			const double t1 = s * std::cos(phi) - sub;

			if(0.0 <= t1 && t1 <= 1.0001f)
				return t1;

			const double t2 = -s * std::cos(phi + pi / 3.0) - sub;

			if(0.0 <= t2 && t2 <= 1.0001f)
				return t2;
			return -s * std::cos(phi - pi / 3.0) - sub;
		}
	}
}

void CRenderTools::RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, int Channels, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result)
{
	const int NumPoints = pPoints->NumPoints();
	if(NumPoints == 0)
	{
		Result = ColorRGBA();
		return;
	}

	if(NumPoints == 1)
	{
		const CEnvPoint *pFirstPoint = pPoints->GetPoint(0);
		Result.r = fx2f(pFirstPoint->m_aValues[0]);
		Result.g = fx2f(pFirstPoint->m_aValues[1]);
		Result.b = fx2f(pFirstPoint->m_aValues[2]);
		Result.a = fx2f(pFirstPoint->m_aValues[3]);
		return;
	}

	const CEnvPoint *pLastPoint = pPoints->GetPoint(NumPoints - 1);
	const int64_t MaxPointTime = (int64_t)pLastPoint->m_Time * std::chrono::nanoseconds(1ms).count();
	if(MaxPointTime > 0) // TODO: remove this check when implementing a IO check for maps(in this case broken envelopes)
		TimeNanos = std::chrono::nanoseconds(TimeNanos.count() % MaxPointTime);
	else
		TimeNanos = decltype(TimeNanos)::zero();

	const double TimeMillis = TimeNanos.count() / (double)std::chrono::nanoseconds(1ms).count();
	for(int i = 0; i < NumPoints - 1; i++)
	{
		const CEnvPoint *pCurrentPoint = pPoints->GetPoint(i);
		const CEnvPoint *pNextPoint = pPoints->GetPoint(i + 1);
		if(TimeMillis >= pCurrentPoint->m_Time && TimeMillis <= pNextPoint->m_Time)
		{
			const float Delta = pNextPoint->m_Time - pCurrentPoint->m_Time;
			float a = (float)(TimeMillis - pCurrentPoint->m_Time) / Delta;

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
				const CEnvPointBezier *pCurrentPointBezier = pPoints->GetBezier(i);
				const CEnvPointBezier *pNextPointBezier = pPoints->GetBezier(i + 1);
				if(pCurrentPointBezier == nullptr || pNextPointBezier == nullptr)
					break; // fallback to linear
				for(int c = 0; c < Channels; c++)
				{
					// monotonic 2d cubic bezier curve
					const vec2 p0 = vec2(pCurrentPoint->m_Time / 1000.0f, fx2f(pCurrentPoint->m_aValues[c]));
					const vec2 p3 = vec2(pNextPoint->m_Time / 1000.0f, fx2f(pNextPoint->m_aValues[c]));

					const vec2 OutTang = vec2(pCurrentPointBezier->m_aOutTangentDeltaX[c] / 1000.0f, fx2f(pCurrentPointBezier->m_aOutTangentDeltaY[c]));
					const vec2 InTang = -vec2(pNextPointBezier->m_aInTangentDeltaX[c] / 1000.0f, fx2f(pNextPointBezier->m_aInTangentDeltaY[c]));
					vec2 p1 = p0 + OutTang;
					vec2 p2 = p3 - InTang;

					// validate bezier curve
					ValidateFCurve(p0, p1, p2, p3);

					// solve x(a) = time for a
					a = clamp(SolveBezier(TimeMillis / 1000.0f, p0.x, p1.x, p2.x, p3.x), 0.0f, 1.0f);

					// value = y(t)
					Result[c] = bezier(p0.y, p1.y, p2.y, p3.y, a);
				}
				return;
			}

			case CURVETYPE_LINEAR: [[fallthrough]];
			default:
				break;
			}

			for(int c = 0; c < Channels; c++)
			{
				const float v0 = fx2f(pCurrentPoint->m_aValues[c]);
				const float v1 = fx2f(pNextPoint->m_aValues[c]);
				Result[c] = v0 + (v1 - v0) * a;
			}

			return;
		}
	}

	Result.r = fx2f(pLastPoint->m_aValues[0]);
	Result.g = fx2f(pLastPoint->m_aValues[1]);
	Result.b = fx2f(pLastPoint->m_aValues[2]);
	Result.a = fx2f(pLastPoint->m_aValues[3]);
}

static void Rotate(CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * std::cos(Rotation) - y * std::sin(Rotation) + pCenter->x);
	pPoint->y = (int)(x * std::sin(Rotation) + y * std::cos(Rotation) + pCenter->y);
}

void CRenderTools::RenderQuads(CQuad *pQuads, int NumQuads, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser)
{
	if(!g_Config.m_ClShowQuads || g_Config.m_ClOverlayEntities == 100)
		return;

	ForceRenderQuads(pQuads, NumQuads, RenderFlags, pfnEval, pUser, (100 - g_Config.m_ClOverlayEntities) / 100.0f);
}

void CRenderTools::ForceRenderQuads(CQuad *pQuads, int NumQuads, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser, float Alpha)
{
	Graphics()->TrianglesBegin();
	float Conv = 1 / 255.0f;
	for(int i = 0; i < NumQuads; i++)
	{
		CQuad *pQuad = &pQuads[i];

		ColorRGBA Color(1.f, 1.f, 1.f, 1.f);
		if(pQuad->m_ColorEnv >= 0)
		{
			pfnEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, Color, pUser);
		}

		if(Color.a <= 0)
			continue;

		bool Opaque = false;
		/* TODO: Analyze quadtexture
		if(a < 0.01f || (q->m_aColors[0].a < 0.01f && q->m_aColors[1].a < 0.01f && q->m_aColors[2].a < 0.01f && q->m_aColors[3].a < 0.01f))
			Opaque = true;
		*/
		if(Opaque && !(RenderFlags & LAYERRENDERFLAG_OPAQUE))
			continue;
		if(!Opaque && !(RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
			continue;

		Graphics()->QuadsSetSubsetFree(
			fx2f(pQuad->m_aTexcoords[0].x), fx2f(pQuad->m_aTexcoords[0].y),
			fx2f(pQuad->m_aTexcoords[1].x), fx2f(pQuad->m_aTexcoords[1].y),
			fx2f(pQuad->m_aTexcoords[2].x), fx2f(pQuad->m_aTexcoords[2].y),
			fx2f(pQuad->m_aTexcoords[3].x), fx2f(pQuad->m_aTexcoords[3].y));

		float OffsetX = 0;
		float OffsetY = 0;
		float Rot = 0;

		// TODO: fix this
		if(pQuad->m_PosEnv >= 0)
		{
			ColorRGBA Channels;
			pfnEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Channels, pUser);
			OffsetX = Channels.r;
			OffsetY = Channels.g;
			Rot = Channels.b / 360.0f * pi * 2;
		}

		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, pQuad->m_aColors[0].r * Conv * Color.r, pQuad->m_aColors[0].g * Conv * Color.g, pQuad->m_aColors[0].b * Conv * Color.b, pQuad->m_aColors[0].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(1, pQuad->m_aColors[1].r * Conv * Color.r, pQuad->m_aColors[1].g * Conv * Color.g, pQuad->m_aColors[1].b * Conv * Color.b, pQuad->m_aColors[1].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(2, pQuad->m_aColors[2].r * Conv * Color.r, pQuad->m_aColors[2].g * Conv * Color.g, pQuad->m_aColors[2].b * Conv * Color.b, pQuad->m_aColors[2].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(3, pQuad->m_aColors[3].r * Conv * Color.r, pQuad->m_aColors[3].g * Conv * Color.g, pQuad->m_aColors[3].b * Conv * Color.b, pQuad->m_aColors[3].a * Conv * Color.a * Alpha)};
		Graphics()->SetColorVertex(Array, 4);

		CPoint *pPoints = pQuad->m_aPoints;

		if(Rot != 0)
		{
			static CPoint aRotated[4];
			aRotated[0] = pQuad->m_aPoints[0];
			aRotated[1] = pQuad->m_aPoints[1];
			aRotated[2] = pQuad->m_aPoints[2];
			aRotated[3] = pQuad->m_aPoints[3];
			pPoints = aRotated;

			Rotate(&pQuad->m_aPoints[4], &aRotated[0], Rot);
			Rotate(&pQuad->m_aPoints[4], &aRotated[1], Rot);
			Rotate(&pQuad->m_aPoints[4], &aRotated[2], Rot);
			Rotate(&pQuad->m_aPoints[4], &aRotated[3], Rot);
		}

		IGraphics::CFreeformItem Freeform(
			fx2f(pPoints[0].x) + OffsetX, fx2f(pPoints[0].y) + OffsetY,
			fx2f(pPoints[1].x) + OffsetX, fx2f(pPoints[1].y) + OffsetY,
			fx2f(pPoints[2].x) + OffsetX, fx2f(pPoints[2].y) + OffsetY,
			fx2f(pPoints[3].x) + OffsetX, fx2f(pPoints[3].y) + OffsetY);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	}
	Graphics()->TrianglesEnd();
}

void CRenderTools::RenderTileRectangle(int RectX, int RectY, int RectW, int RectH,
	unsigned char IndexIn, unsigned char IndexOut,
	float Scale, ColorRGBA Color, int RenderFlags,
	ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	ColorRGBA Channels(1.f, 1.f, 1.f, 1.f);
	if(ColorEnv >= 0)
	{
		pfnEval(ColorEnvOffset, ColorEnv, Channels, pUser);
	}

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r * Channels.r, Color.g * Channels.g, Color.b * Channels.b, Color.a * Channels.a);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

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

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
					IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}
	}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTilemap(CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags,
	ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	ColorRGBA Channels(1.f, 1.f, 1.f, 1.f);
	if(ColorEnv >= 0)
	{
		pfnEval(ColorEnvOffset, ColorEnv, Channels, pUser);
	}

	if(Graphics()->IsTileBufferingEnabled())
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r * Channels.r, Color.g * Channels.g, Color.b * Channels.b, Color.a * Channels.a);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
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
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int c = mx + my * w;

			unsigned char Index = pTiles[c].m_Index;
			if(Index)
			{
				unsigned char Flags = pTiles[c].m_Flags;

				bool Render = false;
				if(Flags & TILEFLAG_OPAQUE && Color.a * Channels.a > 254.0f / 255.0f)
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

					if(Graphics()->IsTileBufferingEnabled())
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

					if(Graphics()->IsTileBufferingEnabled())
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
			x += pTiles[c].m_Skip;
		}
	}

	if(Graphics()->IsTileBufferingEnabled())
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, float Alpha)
{
	if(!g_Config.m_ClTextEntities)
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(mx < 0)
				continue; // mx = 0;
			if(mx >= w)
				continue; // mx = w-1;
			if(my < 0)
				continue; // my = 0;
			if(my >= h)
				continue; // my = h-1;

			int c = mx + my * w;

			unsigned char Index = pTele[c].m_Number;
			if(Index && IsTeleTileNumberUsed(pTele[c].m_Type))
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				TextRender()->Text(mx * Scale - 3.f, (my + ToCenterOffset) * Scale, Size * Scale, aBuf, -1.0f);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSpeedupOverlay(CSpeedupTile *pSpeedup, int w, int h, float Scale, float Alpha)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(mx < 0)
				continue; // mx = 0;
			if(mx >= w)
				continue; // mx = w-1;
			if(my < 0)
				continue; // my = 0;
			if(my >= h)
				continue; // my = h-1;

			int c = mx + my * w;

			int Force = (int)pSpeedup[c].m_Force;
			int MaxSpeed = (int)pSpeedup[c].m_MaxSpeed;
			if(Force)
			{
				// draw arrow
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_Id);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(255.0f, 255.0f, 255.0f, Alpha);

				SelectSprite(SPRITE_SPEEDUP_ARROW);
				Graphics()->QuadsSetRotation(pSpeedup[c].m_Angle * (pi / 180.0f));
				DrawSprite(mx * Scale + 16, my * Scale + 16, 35.0f);

				Graphics()->QuadsEnd();

				if(g_Config.m_ClTextEntities)
				{
					// draw force
					char aBuf[16];
					str_format(aBuf, sizeof(aBuf), "%d", Force);
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
					TextRender()->Text(mx * Scale, (my + 0.5f + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf, -1.0f);
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
					if(MaxSpeed)
					{
						str_format(aBuf, sizeof(aBuf), "%d", MaxSpeed);
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
						TextRender()->Text(mx * Scale, (my + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf, -1.0f);
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
					}
				}
			}
		}
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, float Alpha)
{
	if(!g_Config.m_ClTextEntities)
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(mx < 0)
				continue; // mx = 0;
			if(mx >= w)
				continue; // mx = w-1;
			if(my < 0)
				continue; // my = 0;
			if(my >= h)
				continue; // my = h-1;

			int c = mx + my * w;

			unsigned char Index = pSwitch[c].m_Number;
			if(Index && IsSwitchTileNumberUsed(pSwitch[c].m_Type))
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				TextRender()->Text(mx * Scale, (my + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf, -1.0f);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}

			unsigned char Delay = pSwitch[c].m_Delay;
			if(Delay && IsSwitchTileDelayUsed(pSwitch[c].m_Type))
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", Delay);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				TextRender()->Text(mx * Scale, (my + 0.5f + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf, -1.0f);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, float Alpha)
{
	if(!g_Config.m_ClTextEntities)
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return; // its useless to render text at this distance

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(mx < 0)
				continue; // mx = 0;
			if(mx >= w)
				continue; // mx = w-1;
			if(my < 0)
				continue; // my = 0;
			if(my >= h)
				continue; // my = h-1;

			int c = mx + my * w;

			unsigned char Index = pTune[c].m_Number;
			if(Index)
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				TextRender()->Text(mx * Scale + 11.f, my * Scale + 6.f, Size * Scale / 1.5f - 5.f, aBuf, -1.0f); // numbers shouldn't be too big and in the center of the tile
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
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
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int c = mx + my * w;

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

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
					IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSpeedupmap(CSpeedupTile *pSpeedupTile, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	//Graphics()->TextureSet(img_get(tmap->image));
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	//Graphics()->MapScreen(screen_x0-50, screen_y0-50, screen_x1+50, screen_y1+50);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
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
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int c = mx + my * w;

			unsigned char Index = pSpeedupTile[c].m_Type;
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

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
					IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSwitchmap(CSwitchTile *pSwitchTile, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	//Graphics()->TextureSet(img_get(tmap->image));
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	//Graphics()->MapScreen(screen_x0-50, screen_y0-50, screen_x1+50, screen_y1+50);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
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
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int c = mx + my * w;

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

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
					IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
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
			else
			{
				if(mx < 0)
					continue; // mx = 0;
				if(mx >= w)
					continue; // mx = w-1;
				if(my < 0)
					continue; // my = 0;
				if(my >= h)
					continue; // my = h-1;
			}

			int c = mx + my * w;

			unsigned char Index = pTune[c].m_Type;
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

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
					IGraphics::CQuadItem QuadItem(x * Scale, y * Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}
