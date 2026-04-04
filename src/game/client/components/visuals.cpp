#include "visuals.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <generated/protocol.h>
#include <game/mapitems.h>
#include <game/visual_primitives.h>

#include <base/math.h>
#include <base/vmath.h>

#include <base/color.h>

#include <game/layers.h>

#include <algorithm>

// --- Individual item rendering (geometry only, no Begin/End/TextureSet) ---

void CVisuals::RenderLine(const CVisualItem &Item)
{
	const CNetObj_DDNetVisualLine *pLine = static_cast<const CNetObj_DDNetVisualLine *>(Item.m_pData);

	vec2 From(pLine->m_FromX, pLine->m_FromY);
	vec2 To(pLine->m_ToX, pLine->m_ToY);

	if(Item.m_pPrevData)
	{
		const CNetObj_DDNetVisualLine *pPrevLine = static_cast<const CNetObj_DDNetVisualLine *>(Item.m_pPrevData);
		From = mix(vec2(pPrevLine->m_FromX, pPrevLine->m_FromY), From, m_IntraGameTick);
		To = mix(vec2(pPrevLine->m_ToX, pPrevLine->m_ToY), To, m_IntraGameTick);
	}

	if(pLine->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		From += m_CameraCenter;
		To += m_CameraCenter;
	}

	if(SpringEnabled(pLine->m_Flags))
	{
		float Omega = GetSpringOmega(pLine->m_Flags);
		float Decay = expf(-Omega * m_RenderFrameTime);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, From, To, 0);
		From = SpringDamp(S.m_Pos, S.m_Vel, From, m_RenderFrameTime, Omega, Decay);
		To = SpringDamp(S.m_Pos2, S.m_Vel2, To, m_RenderFrameTime, Omega, Decay);
	}

	Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pLine->m_Color));

	int Width = pLine->m_Width;
	if(Width <= 1)
	{
		IGraphics::CLineItem Line(From.x, From.y, To.x, To.y);
		Graphics()->LinesDraw(&Line, 1);
	}
	else
	{
		vec2 Dir = To - From;
		float Len = length(Dir);
		if(Len > 0.0f)
			Dir = Dir / Len;
		else
			Dir = vec2(1.0f, 0.0f);

		vec2 Perp = vec2(-Dir.y, Dir.x) * (Width / 2.0f);

		IGraphics::CFreeformItem Freeform(
			From.x - Perp.x, From.y - Perp.y,
			From.x + Perp.x, From.y + Perp.y,
			To.x - Perp.x, To.y - Perp.y,
			To.x + Perp.x, To.y + Perp.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		int Cap = UnpackLineCap(pLine->m_Flags);
		if(Cap == LINECAP_ROUND)
		{
			float HalfWidth = Width / 2.0f;
			int Segments = maximum(8, Width / 2);
			for(int i = 0; i < Segments; i++)
			{
				float a1 = pi / 2.0f + (float)i / Segments * pi;
				float a2 = pi / 2.0f + (float)(i + 1) / Segments * pi;
				vec2 p1 = From + vec2(cosf(a1) * Dir.x - sinf(a1) * Dir.y, sinf(a1) * Dir.x + cosf(a1) * Dir.y) * HalfWidth;
				vec2 p2 = From + vec2(cosf(a2) * Dir.x - sinf(a2) * Dir.y, sinf(a2) * Dir.x + cosf(a2) * Dir.y) * HalfWidth;
				IGraphics::CFreeformItem CapItem(From.x, From.y, From.x, From.y, p1.x, p1.y, p2.x, p2.y);
				Graphics()->QuadsDrawFreeform(&CapItem, 1);
			}
			for(int i = 0; i < Segments; i++)
			{
				float a1 = -pi / 2.0f + (float)i / Segments * pi;
				float a2 = -pi / 2.0f + (float)(i + 1) / Segments * pi;
				vec2 p1 = To + vec2(cosf(a1) * Dir.x - sinf(a1) * Dir.y, sinf(a1) * Dir.x + cosf(a1) * Dir.y) * HalfWidth;
				vec2 p2 = To + vec2(cosf(a2) * Dir.x - sinf(a2) * Dir.y, sinf(a2) * Dir.x + cosf(a2) * Dir.y) * HalfWidth;
				IGraphics::CFreeformItem CapItem(To.x, To.y, To.x, To.y, p1.x, p1.y, p2.x, p2.y);
				Graphics()->QuadsDrawFreeform(&CapItem, 1);
			}
		}
		else if(Cap == LINECAP_SQUARE)
		{
			vec2 Ext = Dir * (Width / 2.0f);
			IGraphics::CFreeformItem StartCap(
				From.x - Perp.x - Ext.x, From.y - Perp.y - Ext.y,
				From.x + Perp.x - Ext.x, From.y + Perp.y - Ext.y,
				From.x - Perp.x, From.y - Perp.y,
				From.x + Perp.x, From.y + Perp.y);
			Graphics()->QuadsDrawFreeform(&StartCap, 1);
			IGraphics::CFreeformItem EndCap(
				To.x - Perp.x, To.y - Perp.y,
				To.x + Perp.x, To.y + Perp.y,
				To.x - Perp.x + Ext.x, To.y - Perp.y + Ext.y,
				To.x + Perp.x + Ext.x, To.y + Perp.y + Ext.y);
			Graphics()->QuadsDrawFreeform(&EndCap, 1);
		}
	}
}

void CVisuals::RenderCircle(const CVisualItem &Item)
{
	const CNetObj_DDNetVisualCircle *pCircle = static_cast<const CNetObj_DDNetVisualCircle *>(Item.m_pData);

	float CenterX = (float)pCircle->m_X;
	float CenterY = (float)pCircle->m_Y;
	float Radius = (float)pCircle->m_Radius;

	if(Item.m_pPrevData)
	{
		const CNetObj_DDNetVisualCircle *pPrevCircle = static_cast<const CNetObj_DDNetVisualCircle *>(Item.m_pPrevData);
		CenterX = mix((float)pPrevCircle->m_X, CenterX, m_IntraGameTick);
		CenterY = mix((float)pPrevCircle->m_Y, CenterY, m_IntraGameTick);
		Radius = mix((float)pPrevCircle->m_Radius, Radius, m_IntraGameTick);
	}

	if(pCircle->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		CenterX += m_CameraCenter.x;
		CenterY += m_CameraCenter.y;
	}

	if(SpringEnabled(pCircle->m_Flags))
	{
		float Omega = GetSpringOmega(pCircle->m_Flags);
		float Decay = expf(-Omega * m_RenderFrameTime);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, vec2(CenterX, CenterY), vec2(0, 0), 0);
		vec2 Result = SpringDamp(S.m_Pos, S.m_Vel, vec2(CenterX, CenterY), m_RenderFrameTime, Omega, Decay);
		CenterX = Result.x;
		CenterY = Result.y;
	}

	bool Filled = UnpackCircleFilled(pCircle->m_Flags);
	int Style = UnpackCircleStyle(pCircle->m_Flags);

	Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pCircle->m_Color));

	if(Style == CIRCLESTYLE_RADIAL_PROGRESS)
	{
		float Progress = UnpackCircleProgress(pCircle->m_Flags) / 255.0f;
		float ArcAngle = Progress * 2.0f * pi;
		int Segments = maximum(16, (int)(Radius / 4.0f));
		int ArcSegments = maximum(1, (int)(Segments * Progress));
		// Start from top (-pi/2)
		float StartAngle = -pi / 2.0f;

		if(Filled)
		{
			// Pie: triangle fan from center
			for(int i = 0; i < ArcSegments; i++)
			{
				float a1 = StartAngle + ArcAngle * (float)i / ArcSegments;
				float a2 = StartAngle + ArcAngle * (float)(i + 1) / ArcSegments;
				IGraphics::CFreeformItem Tri(
					CenterX, CenterY,
					CenterX, CenterY,
					CenterX + cosf(a1) * Radius, CenterY + sinf(a1) * Radius,
					CenterX + cosf(a2) * Radius, CenterY + sinf(a2) * Radius);
				Graphics()->QuadsDrawFreeform(&Tri, 1);
			}
		}
		else
		{
			// Arc: ring outline for the progress portion
			float OutlineWidth = maximum(1.0f, (float)pCircle->m_Width);
			float Inner = Radius - OutlineWidth / 2.0f;
			float Outer = Radius + OutlineWidth / 2.0f;

			for(int i = 0; i < ArcSegments; i++)
			{
				float a1 = StartAngle + ArcAngle * (float)i / ArcSegments;
				float a2 = StartAngle + ArcAngle * (float)(i + 1) / ArcSegments;
				IGraphics::CFreeformItem Ring(
					CenterX + cosf(a1) * Inner, CenterY + sinf(a1) * Inner,
					CenterX + cosf(a1) * Outer, CenterY + sinf(a1) * Outer,
					CenterX + cosf(a2) * Inner, CenterY + sinf(a2) * Inner,
					CenterX + cosf(a2) * Outer, CenterY + sinf(a2) * Outer);
				Graphics()->QuadsDrawFreeform(&Ring, 1);
			}
		}
	}
	else
	{
		int Segments = UnpackCircleSegments(pCircle->m_Flags);
		if(Segments == 0)
			Segments = maximum(16, (int)(Radius / 4.0f));

		if(Filled)
		{
			Graphics()->DrawCircle(CenterX, CenterY, Radius, Segments);
		}
		else
		{
			float OutlineWidth = maximum(1.0f, (float)pCircle->m_Width);
		float Inner = Radius - OutlineWidth / 2.0f;
		float Outer = Radius + OutlineWidth / 2.0f;

		for(int i = 0; i < Segments; i++)
		{
			float a1 = (float)i / Segments * 2.0f * pi;
			float a2 = (float)(i + 1) / Segments * 2.0f * pi;
			IGraphics::CFreeformItem Ring(
				CenterX + cosf(a1) * Inner, CenterY + sinf(a1) * Inner,
				CenterX + cosf(a1) * Outer, CenterY + sinf(a1) * Outer,
				CenterX + cosf(a2) * Inner, CenterY + sinf(a2) * Inner,
				CenterX + cosf(a2) * Outer, CenterY + sinf(a2) * Outer);
			Graphics()->QuadsDrawFreeform(&Ring, 1);
		}
		}
	}
}

void CVisuals::RenderQuad(const CVisualItem &Item)
{
	const CNetObj_DDNetVisualQuad *pQuad = static_cast<const CNetObj_DDNetVisualQuad *>(Item.m_pData);

	float CenterX = (float)pQuad->m_X;
	float CenterY = (float)pQuad->m_Y;
	float HalfW = pQuad->m_W / 2.0f;
	float HalfH = pQuad->m_H / 2.0f;
	float AngleRad = (pQuad->m_Angle / 256.0f) * (pi / 180.0f);

	if(Item.m_pPrevData)
	{
		const CNetObj_DDNetVisualQuad *pPrevQuad = static_cast<const CNetObj_DDNetVisualQuad *>(Item.m_pPrevData);
		CenterX = mix((float)pPrevQuad->m_X, CenterX, m_IntraGameTick);
		CenterY = mix((float)pPrevQuad->m_Y, CenterY, m_IntraGameTick);
		HalfW = mix(pPrevQuad->m_W / 2.0f, HalfW, m_IntraGameTick);
		HalfH = mix(pPrevQuad->m_H / 2.0f, HalfH, m_IntraGameTick);
		AngleRad = mix((pPrevQuad->m_Angle / 256.0f) * (pi / 180.0f), AngleRad, m_IntraGameTick);
	}

	if(pQuad->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		CenterX += m_CameraCenter.x;
		CenterY += m_CameraCenter.y;
	}

	if(SpringEnabled(pQuad->m_Flags))
	{
		float Omega = GetSpringOmega(pQuad->m_Flags);
		float Decay = expf(-Omega * m_RenderFrameTime);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, vec2(CenterX, CenterY), vec2(0, 0), AngleRad);
		vec2 Result = SpringDamp(S.m_Pos, S.m_Vel, vec2(CenterX, CenterY), m_RenderFrameTime, Omega, Decay);
		CenterX = Result.x;
		CenterY = Result.y;
		AngleRad = SpringDampAngle(S.m_Angle, S.m_AngleVel, AngleRad, m_RenderFrameTime, Omega, Decay);
	}

	IGraphics::CTextureHandle QuadTex = GetImageTexture(pQuad->m_ImageIndex);
	bool HasTexture = !QuadTex.IsNullTexture();
	bool Filled = UnpackShapeFilled(pQuad->m_Flags) || HasTexture;

	Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pQuad->m_Color));

	if(Filled)
	{
		// Reset UV subset after tiles that may have changed it within the same batch
		if(HasTexture)
			Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadsSetRotation(AngleRad);
		IGraphics::CQuadItem QuadItem(CenterX - HalfW, CenterY - HalfH, HalfW * 2.0f, HalfH * 2.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsSetRotation(0.0f);
	}
	else
	{
		float OutlineWidth = maximum(1.0f, (float)pQuad->m_Width);
		float Cos = cosf(AngleRad);
		float Sin = sinf(AngleRad);

		vec2 Corners[4] = {
			vec2(CenterX + (-HalfW) * Cos - (-HalfH) * Sin, CenterY + (-HalfW) * Sin + (-HalfH) * Cos),
			vec2(CenterX + (HalfW) * Cos - (-HalfH) * Sin, CenterY + (HalfW) * Sin + (-HalfH) * Cos),
			vec2(CenterX + (HalfW) * Cos - (HalfH) * Sin, CenterY + (HalfW) * Sin + (HalfH) * Cos),
			vec2(CenterX + (-HalfW) * Cos - (HalfH) * Sin, CenterY + (-HalfW) * Sin + (HalfH) * Cos),
		};

		for(int i = 0; i < 4; i++)
		{
			vec2 A = Corners[i];
			vec2 B = Corners[(i + 1) % 4];
			vec2 EdgeDir = normalize(B - A);
			vec2 EdgePerp = vec2(-EdgeDir.y, EdgeDir.x) * (OutlineWidth / 2.0f);

			IGraphics::CFreeformItem Edge(
				A.x - EdgePerp.x, A.y - EdgePerp.y,
				A.x + EdgePerp.x, A.y + EdgePerp.y,
				B.x - EdgePerp.x, B.y - EdgePerp.y,
				B.x + EdgePerp.x, B.y + EdgePerp.y);
			Graphics()->QuadsDrawFreeform(&Edge, 1);
		}
	}
}

void CVisuals::RenderTile(const CVisualItem &Item)
{
	const CNetObj_DDNetVisualTile *pTile = static_cast<const CNetObj_DDNetVisualTile *>(Item.m_pData);

	IGraphics::CTextureHandle TileTex = GetImageTexture(pTile->m_ImageIndex);
	if(TileTex.IsNullTexture())
		return;
	if(pTile->m_TileIndex < 0 || pTile->m_TileIndex > 255)
		return;

	float CenterX = (float)pTile->m_X;
	float CenterY = (float)pTile->m_Y;
	float AngleRad = (pTile->m_Angle / 256.0f) * (pi / 180.0f);

	if(Item.m_pPrevData)
	{
		const CNetObj_DDNetVisualTile *pPrevTile = static_cast<const CNetObj_DDNetVisualTile *>(Item.m_pPrevData);
		CenterX = mix((float)pPrevTile->m_X, CenterX, m_IntraGameTick);
		CenterY = mix((float)pPrevTile->m_Y, CenterY, m_IntraGameTick);
		AngleRad = mix((pPrevTile->m_Angle / 256.0f) * (pi / 180.0f), AngleRad, m_IntraGameTick);
	}

	if(pTile->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		CenterX += m_CameraCenter.x;
		CenterY += m_CameraCenter.y;
	}

	if(SpringEnabled(pTile->m_Flags))
	{
		float Omega = GetSpringOmega(pTile->m_Flags);
		float Decay = expf(-Omega * m_RenderFrameTime);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, vec2(CenterX, CenterY), vec2(0, 0), AngleRad);
		vec2 Result = SpringDamp(S.m_Pos, S.m_Vel, vec2(CenterX, CenterY), m_RenderFrameTime, Omega, Decay);
		CenterX = Result.x;
		CenterY = Result.y;
		AngleRad = SpringDampAngle(S.m_Angle, S.m_AngleVel, AngleRad, m_RenderFrameTime, Omega, Decay);
	}

	int Col = pTile->m_TileIndex % 16;
	int Row = pTile->m_TileIndex / 16;
	float u0 = Col / 16.0f;
	float v0 = Row / 16.0f;
	float u1 = (Col + 1) / 16.0f;
	float v1 = (Row + 1) / 16.0f;

	int TileFlags = pTile->m_Flags;
	if(TileFlags & TILEFLAG_XFLIP)
		std::swap(u0, u1);
	if(TileFlags & TILEFLAG_YFLIP)
		std::swap(v0, v1);

	Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pTile->m_Color));
	Graphics()->QuadsSetSubset(u0, v0, u1, v1);

	float TotalAngle = AngleRad;
	if(TileFlags & TILEFLAG_ROTATE)
		TotalAngle += pi / 2.0f;

	Graphics()->QuadsSetRotation(TotalAngle);

	float HalfW = pTile->m_W / 2.0f;
	float HalfH = pTile->m_H / 2.0f;
	IGraphics::CQuadItem QuadItem(CenterX - HalfW, CenterY - HalfH, (float)pTile->m_W, (float)pTile->m_H);
	Graphics()->QuadsDrawTL(&QuadItem, 1);

	Graphics()->QuadsSetRotation(0.0f);
}

// --- Spring damping ---

vec2 CVisuals::SpringDamp(vec2 &Pos, vec2 &Vel, vec2 Target, float Dt, float Omega, float Decay)
{
	vec2 Delta = Pos - Target;
	Pos = Target + (Delta + (Vel + Delta * Omega) * Dt) * Decay;
	Vel = (Vel - (Vel + Delta * Omega) * Omega * Dt) * Decay;
	return Pos;
}

float CVisuals::SpringDampAngle(float &Angle, float &Vel, float Target, float Dt, float Omega, float Decay)
{
	float Delta = Angle - Target;
	Angle = Target + (Delta + (Vel + Delta * Omega) * Dt) * Decay;
	Vel = (Vel - (Vel + Delta * Omega) * Omega * Dt) * Decay;
	return Angle;
}

CSpringState &CVisuals::FindOrCreateSpring(int SnapId, vec2 Pos, vec2 Pos2, float Angle)
{
	auto [it, Inserted] = m_Springs.emplace(SnapId, CSpringState{});
	if(Inserted)
	{
		it->second.m_Pos = Pos;
		it->second.m_Vel = vec2(0, 0);
		it->second.m_Pos2 = Pos2;
		it->second.m_Vel2 = vec2(0, 0);
		it->second.m_Angle = Angle;
		it->second.m_AngleVel = 0;
	}
	it->second.m_Generation = m_SpringGeneration;
	return it->second;
}

void CVisuals::CleanupSprings()
{
	for(auto it = m_Springs.begin(); it != m_Springs.end();)
	{
		if(it->second.m_Generation != m_SpringGeneration)
			it = m_Springs.erase(it);
		else
			++it;
	}
}

// --- Snap data extraction helpers ---

static int GetFlagsFromSnap(int Type, const void *pData)
{
	if(Type == NETOBJTYPE_DDNETVISUALLINE)
		return static_cast<const CNetObj_DDNetVisualLine *>(pData)->m_Flags;
	if(Type == NETOBJTYPE_DDNETVISUALCIRCLE)
		return static_cast<const CNetObj_DDNetVisualCircle *>(pData)->m_Flags;
	if(Type == NETOBJTYPE_DDNETVISUALQUAD)
		return static_cast<const CNetObj_DDNetVisualQuad *>(pData)->m_Flags;
	if(Type == NETOBJTYPE_DDNETVISUALTILE)
		return static_cast<const CNetObj_DDNetVisualTile *>(pData)->m_Flags;
	return 0;
}

static int GetRenderOrderFromSnap(int Type, const void *pData)
{
	if(Type == NETOBJTYPE_DDNETVISUALLINE)
		return static_cast<const CNetObj_DDNetVisualLine *>(pData)->m_RenderOrder;
	if(Type == NETOBJTYPE_DDNETVISUALCIRCLE)
		return static_cast<const CNetObj_DDNetVisualCircle *>(pData)->m_RenderOrder;
	if(Type == NETOBJTYPE_DDNETVISUALQUAD)
		return static_cast<const CNetObj_DDNetVisualQuad *>(pData)->m_RenderOrder;
	if(Type == NETOBJTYPE_DDNETVISUALTILE)
		return static_cast<const CNetObj_DDNetVisualTile *>(pData)->m_RenderOrder;
	return RENDERORDER_DEFAULT;
}

// Compute batch key for draw-call batching:
//   <0 = lines mode (thin lines), 0 = quads no texture, >0 = quads with texture (imageIndex + 1)
static int ComputeBatchKey(int Type, const void *pData)
{
	switch(Type)
	{
	case NETOBJTYPE_DDNETVISUALLINE:
		return static_cast<const CNetObj_DDNetVisualLine *>(pData)->m_Width <= 1 ? -1 : 0;
	case NETOBJTYPE_DDNETVISUALCIRCLE:
		return 0;
	case NETOBJTYPE_DDNETVISUALQUAD:
	{
		int Idx = static_cast<const CNetObj_DDNetVisualQuad *>(pData)->m_ImageIndex;
		return Idx >= 0 ? Idx + 1 : 0;
	}
	case NETOBJTYPE_DDNETVISUALTILE:
	{
		int Idx = static_cast<const CNetObj_DDNetVisualTile *>(pData)->m_ImageIndex;
		return Idx >= 0 ? Idx + 1 : 0;
	}
	default:
		return 0;
	}
}

// --- Rendering dispatch ---

void CVisuals::RenderItem(const CVisualItem &Item)
{
	if(Item.m_Type == NETOBJTYPE_DDNETVISUALLINE)
		RenderLine(Item);
	else if(Item.m_Type == NETOBJTYPE_DDNETVISUALCIRCLE)
		RenderCircle(Item);
	else if(Item.m_Type == NETOBJTYPE_DDNETVISUALQUAD)
		RenderQuad(Item);
	else if(Item.m_Type == NETOBJTYPE_DDNETVISUALTILE)
		RenderTile(Item);
}

void CVisuals::RenderBatchedItems(const CVisualItem *pItems, int NumItems)
{
	bool ScreenSpaceActive = false;
	float SavedScreen[4] = {0};
	Graphics()->GetScreen(&SavedScreen[0], &SavedScreen[1], &SavedScreen[2], &SavedScreen[3]);

	int i = 0;
	while(i < NumItems)
	{
		int BatchKey = pItems[i].m_BatchKey;
		bool IsScreenSpace = (pItems[i].m_Flags & VISUALFLAG_SCREEN_SPACE) != 0;

		if(IsScreenSpace && !ScreenSpaceActive)
		{
			Graphics()->MapScreen(0, 0, 1000.0f * Graphics()->ScreenAspect(), 1000.0f);
			ScreenSpaceActive = true;
		}
		else if(!IsScreenSpace && ScreenSpaceActive)
		{
			Graphics()->MapScreen(SavedScreen[0], SavedScreen[1], SavedScreen[2], SavedScreen[3]);
			ScreenSpaceActive = false;
		}

		if(BatchKey < 0)
		{
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
		}
		else
		{
			if(BatchKey > 0)
			{
				IGraphics::CTextureHandle Tex = GetImageTexture(BatchKey - 1);
				if(!Tex.IsNullTexture())
					Graphics()->TextureSet(Tex);
				else
					Graphics()->TextureClear();
			}
			else
			{
				Graphics()->TextureClear();
			}
			Graphics()->QuadsBegin();
		}

		while(i < NumItems && pItems[i].m_BatchKey == BatchKey &&
			((pItems[i].m_Flags & VISUALFLAG_SCREEN_SPACE) != 0) == IsScreenSpace)
		{
			RenderItem(pItems[i]);
			i++;
		}

		if(BatchKey < 0)
			Graphics()->LinesEnd();
		else
			Graphics()->QuadsEnd();
	}

	// Restore mapping if we ended in screen-space mode
	if(ScreenSpaceActive)
		Graphics()->MapScreen(SavedScreen[0], SavedScreen[1], SavedScreen[2], SavedScreen[3]);
}

// --- Texture cache ---

IGraphics::CTextureHandle CVisuals::GetImageTexture(int ImageIndex)
{
	if(ImageIndex < 0 || ImageIndex >= GameClient()->m_MapImages.Num())
		return IGraphics::CTextureHandle();

	auto it = m_TexCache.find(ImageIndex);
	if(it != m_TexCache.end())
		return it->second;

	IMap *pMap = GameClient()->Map();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
	if(ImageIndex >= Num)
		return IGraphics::CTextureHandle();

	const CMapItemImage_v2 *pImg = static_cast<const CMapItemImage_v2 *>(pMap->GetItem(Start + ImageIndex));
	const char *pName = pMap->GetDataString(pImg->m_ImageName);
	if(pName == nullptr || pName[0] == '\0')
		pName = "(visual)";

	IGraphics::CTextureHandle Handle;
	if(pImg->m_External)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "mapres/%s.png", pName);
		Handle = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
	}
	else
	{
		CImageInfo ImageInfo;
		ImageInfo.m_Width = pImg->m_Width;
		ImageInfo.m_Height = pImg->m_Height;
		ImageInfo.m_Format = CImageInfo::FORMAT_RGBA;
		ImageInfo.m_pData = static_cast<uint8_t *>(pMap->GetData(pImg->m_ImageData));
		if(ImageInfo.m_pData && (size_t)pMap->GetDataSize(pImg->m_ImageData) >= ImageInfo.DataSize())
		{
			char aTexName[IO_MAX_PATH_LENGTH];
			str_format(aTexName, sizeof(aTexName), "visual: %s", pName);
			Handle = Graphics()->LoadTextureRaw(ImageInfo, 0, aTexName);
		}
		pMap->UnloadData(pImg->m_ImageData);
	}
	pMap->UnloadData(pImg->m_ImageName);
	m_TexCache[ImageIndex] = Handle;
	return Handle;
}

// --- Lifecycle ---

void CVisuals::OnMapLoad()
{
	for(auto &[Index, Handle] : m_TexCache)
	{
		if(!Handle.IsNullTexture())
			Graphics()->UnloadTexture(&Handle);
	}
	m_TexCache.clear();
	m_Springs.clear();
	m_SpringGeneration = 0;

	m_GameGroupIndex = -1;
	CLayers *pLayers = GameClient()->Layers();
	if(pLayers)
	{
		CMapItemGroup *pGameGroup = pLayers->GameGroup();
		for(int i = 0; i < pLayers->NumGroups(); i++)
		{
			if(pLayers->GetGroup(i) == pGameGroup)
			{
				m_GameGroupIndex = i;
				break;
			}
		}
	}
}

void CVisuals::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClShowVisuals)
	{
		m_vVisualItems.clear();
		return;
	}

	// Cache per-frame values to avoid repeated virtual calls
	m_IntraGameTick = Client()->IntraGameTick(g_Config.m_ClDummy);
	m_RenderFrameTime = Client()->RenderFrameTime();
	m_CameraCenter = GameClient()->m_Camera.m_Center;

	// Collect all visual items (world-space and screen-space unified)
	m_vVisualItems.clear();

	for(const CSnapEntities &Ent : GameClient()->SnapEntities())
	{
		const IClient::CSnapItem &Item = Ent.m_Item;
		if(Item.m_Type != NETOBJTYPE_DDNETVISUALLINE &&
			Item.m_Type != NETOBJTYPE_DDNETVISUALCIRCLE &&
			Item.m_Type != NETOBJTYPE_DDNETVISUALQUAD &&
			Item.m_Type != NETOBJTYPE_DDNETVISUALTILE)
			continue;

		CVisualItem VisItem;
		VisItem.m_Type = Item.m_Type;
		VisItem.m_Id = Item.m_Id;
		VisItem.m_pData = Item.m_pData;
		VisItem.m_Flags = GetFlagsFromSnap(Item.m_Type, Item.m_pData);
		VisItem.m_pPrevData = (VisItem.m_Flags & VISUALFLAG_NO_INTERPOLATION) ? nullptr :
			Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id);
		VisItem.m_RenderOrder = GetRenderOrderFromSnap(Item.m_Type, Item.m_pData);
		VisItem.m_BatchKey = ComputeBatchKey(Item.m_Type, Item.m_pData);

		int GroupIndex = RenderOrderGroup(VisItem.m_RenderOrder);
		if(GroupIndex == RENDERORDER_GROUP_GAME)
			GroupIndex = m_GameGroupIndex;
		VisItem.m_GroupIndex = GroupIndex;
		m_vVisualItems.push_back(VisItem);
	}

	// Sort by (GroupIndex, RenderOrder, BatchKey) for group lookup + draw batching
	if(m_vVisualItems.size() > 1)
	{
		std::sort(m_vVisualItems.begin(), m_vVisualItems.end(), [](const CVisualItem &a, const CVisualItem &b) {
			if(a.m_GroupIndex != b.m_GroupIndex)
				return a.m_GroupIndex < b.m_GroupIndex;
			if(a.m_RenderOrder != b.m_RenderOrder)
				return a.m_RenderOrder < b.m_RenderOrder;
			return a.m_BatchKey < b.m_BatchKey;
		});
	}

	// Generation-based spring cleanup (no per-frame allocation)
	m_SpringGeneration++;
	for(const CVisualItem &Item : m_vVisualItems)
	{
		auto it = m_Springs.find(Item.m_Id);
		if(it != m_Springs.end())
			it->second.m_Generation = m_SpringGeneration;
	}
	CleanupSprings();
}

void CVisuals::RenderForGroup(int GroupId)
{
	if(m_vVisualItems.empty())
		return;

	// Binary search for the group's range in the sorted vector
	auto begin = std::lower_bound(m_vVisualItems.begin(), m_vVisualItems.end(), GroupId,
		[](const CVisualItem &Item, int Group) { return Item.m_GroupIndex < Group; });
	auto end = std::upper_bound(begin, m_vVisualItems.end(), GroupId,
		[](int Group, const CVisualItem &Item) { return Group < Item.m_GroupIndex; });

	int Count = (int)(end - begin);
	if(Count > 0)
		RenderBatchedItems(&*begin, Count);
}
