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

void CVisuals::RenderLine(const CVisualItem &Item)
{
	const CNetObj_DDNetVisualLine *pLine = static_cast<const CNetObj_DDNetVisualLine *>(Item.m_pData);

	vec2 From(pLine->m_FromX, pLine->m_FromY);
	vec2 To(pLine->m_ToX, pLine->m_ToY);

	// Interpolate with previous snapshot
	if(Item.m_pPrevData)
	{
		const CNetObj_DDNetVisualLine *pPrevLine = static_cast<const CNetObj_DDNetVisualLine *>(Item.m_pPrevData);
		float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);
		From = mix(vec2(pPrevLine->m_FromX, pPrevLine->m_FromY), From, Intra);
		To = mix(vec2(pPrevLine->m_ToX, pPrevLine->m_ToY), To, Intra);
	}

	// Camera-relative: offset by camera center
	if(pLine->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		vec2 CamCenter = GameClient()->m_Camera.m_Center;
		From += CamCenter;
		To += CamCenter;
	}

	// Spring damping for world-space visuals
	if(!(pLine->m_Flags & (VISUALFLAG_SCREEN_SPACE | VISUALFLAG_CAMERA_RELATIVE)))
	{
		float Dt = Client()->RenderFrameTime();
		float Omega = GetSpringOmega(pLine->m_Flags);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, From, To, 0);
		From = SpringDamp(S.m_Pos, S.m_Vel, From, Dt, Omega);
		To = SpringDamp(S.m_Pos2, S.m_Vel2, To, Dt, Omega);
	}

	int Width = pLine->m_Width;

	if(Width <= 1)
	{
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pLine->m_Color));
		IGraphics::CLineItem Line(From.x, From.y, To.x, To.y);
		Graphics()->LinesDraw(&Line, 1);
		Graphics()->LinesEnd();
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

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pLine->m_Color));

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
			// Start cap (semicircle behind From)
			for(int i = 0; i < Segments; i++)
			{
				float a1 = pi / 2.0f + (float)i / Segments * pi;
				float a2 = pi / 2.0f + (float)(i + 1) / Segments * pi;
				vec2 p1 = From + vec2(cosf(a1) * Dir.x - sinf(a1) * Dir.y, sinf(a1) * Dir.x + cosf(a1) * Dir.y) * HalfWidth;
				vec2 p2 = From + vec2(cosf(a2) * Dir.x - sinf(a2) * Dir.y, sinf(a2) * Dir.x + cosf(a2) * Dir.y) * HalfWidth;
				IGraphics::CFreeformItem CapItem(From.x, From.y, From.x, From.y, p1.x, p1.y, p2.x, p2.y);
				Graphics()->QuadsDrawFreeform(&CapItem, 1);
			}
			// End cap (semicircle past To)
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

		Graphics()->QuadsEnd();
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
		float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);
		CenterX = mix((float)pPrevCircle->m_X, CenterX, Intra);
		CenterY = mix((float)pPrevCircle->m_Y, CenterY, Intra);
		Radius = mix((float)pPrevCircle->m_Radius, Radius, Intra);
	}

	// Camera-relative: offset by camera center
	if(pCircle->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		CenterX += GameClient()->m_Camera.m_Center.x;
		CenterY += GameClient()->m_Camera.m_Center.y;
	}

	// Spring damping for world-space visuals
	if(!(pCircle->m_Flags & (VISUALFLAG_SCREEN_SPACE | VISUALFLAG_CAMERA_RELATIVE)))
	{
		float Dt = Client()->RenderFrameTime();
		float Omega = GetSpringOmega(pCircle->m_Flags);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, vec2(CenterX, CenterY), vec2(0, 0), 0);
		vec2 Result = SpringDamp(S.m_Pos, S.m_Vel, vec2(CenterX, CenterY), Dt, Omega);
		CenterX = Result.x;
		CenterY = Result.y;
	}

	bool Filled = UnpackShapeFilled(pCircle->m_Flags);
	int Segments = UnpackCircleSegments(pCircle->m_Flags);
	if(Segments == 0)
		Segments = maximum(16, (int)(Radius / 4.0f));

	Graphics()->TextureClear();

	if(Filled)
	{
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pCircle->m_Color));
		for(int i = 0; i < Segments; i++)
		{
			float a1 = (float)i / Segments * 2.0f * pi;
			float a2 = (float)(i + 1) / Segments * 2.0f * pi;
			IGraphics::CFreeformItem Triangle(
				CenterX, CenterY,
				CenterX, CenterY,
				CenterX + cosf(a1) * Radius, CenterY + sinf(a1) * Radius,
				CenterX + cosf(a2) * Radius, CenterY + sinf(a2) * Radius);
			Graphics()->QuadsDrawFreeform(&Triangle, 1);
		}
		Graphics()->QuadsEnd();
	}
	else
	{
		float OutlineWidth = maximum(1.0f, (float)pCircle->m_Width);
		float Inner = Radius - OutlineWidth / 2.0f;
		float Outer = Radius + OutlineWidth / 2.0f;

		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pCircle->m_Color));
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
		Graphics()->QuadsEnd();
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
		float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);
		CenterX = mix((float)pPrevQuad->m_X, CenterX, Intra);
		CenterY = mix((float)pPrevQuad->m_Y, CenterY, Intra);
		HalfW = mix(pPrevQuad->m_W / 2.0f, HalfW, Intra);
		HalfH = mix(pPrevQuad->m_H / 2.0f, HalfH, Intra);
		AngleRad = mix((pPrevQuad->m_Angle / 256.0f) * (pi / 180.0f), AngleRad, Intra);
	}

	// Camera-relative: offset by camera center
	if(pQuad->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		CenterX += GameClient()->m_Camera.m_Center.x;
		CenterY += GameClient()->m_Camera.m_Center.y;
	}

	// Spring damping for world-space visuals
	if(!(pQuad->m_Flags & (VISUALFLAG_SCREEN_SPACE | VISUALFLAG_CAMERA_RELATIVE)))
	{
		float Dt = Client()->RenderFrameTime();
		float Omega = GetSpringOmega(pQuad->m_Flags);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, vec2(CenterX, CenterY), vec2(0, 0), AngleRad);
		vec2 Result = SpringDamp(S.m_Pos, S.m_Vel, vec2(CenterX, CenterY), Dt, Omega);
		CenterX = Result.x;
		CenterY = Result.y;
		AngleRad = SpringDampAngle(S.m_Angle, S.m_AngleVel, AngleRad, Dt, Omega);
	}

	IGraphics::CTextureHandle QuadTex = GetImageTexture(pQuad->m_ImageIndex);
	bool HasTexture = !QuadTex.IsNullTexture();
	bool Filled = UnpackShapeFilled(pQuad->m_Flags) || HasTexture;

	if(HasTexture)
		Graphics()->TextureSet(QuadTex);
	else
		Graphics()->TextureClear();

	if(Filled)
	{
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pQuad->m_Color));
		Graphics()->QuadsSetRotation(AngleRad);

		IGraphics::CQuadItem QuadItem(CenterX - HalfW, CenterY - HalfH, HalfW * 2.0f, HalfH * 2.0f);
		Graphics()->QuadsDrawTL(&QuadItem, 1);

		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
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

		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA::UnpackAlphaLast<ColorRGBA>(pQuad->m_Color));
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
		Graphics()->QuadsEnd();
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
		float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);
		CenterX = mix((float)pPrevTile->m_X, CenterX, Intra);
		CenterY = mix((float)pPrevTile->m_Y, CenterY, Intra);
		AngleRad = mix((pPrevTile->m_Angle / 256.0f) * (pi / 180.0f), AngleRad, Intra);
	}

	// Camera-relative: offset by camera center
	if(pTile->m_Flags & VISUALFLAG_CAMERA_RELATIVE)
	{
		CenterX += GameClient()->m_Camera.m_Center.x;
		CenterY += GameClient()->m_Camera.m_Center.y;
	}

	// Spring damping for world-space visuals
	if(!(pTile->m_Flags & (VISUALFLAG_SCREEN_SPACE | VISUALFLAG_CAMERA_RELATIVE)))
	{
		float Dt = Client()->RenderFrameTime();
		float Omega = GetSpringOmega(pTile->m_Flags);
		CSpringState &S = FindOrCreateSpring(Item.m_Id, vec2(CenterX, CenterY), vec2(0, 0), AngleRad);
		vec2 Result = SpringDamp(S.m_Pos, S.m_Vel, vec2(CenterX, CenterY), Dt, Omega);
		CenterX = Result.x;
		CenterY = Result.y;
		AngleRad = SpringDampAngle(S.m_Angle, S.m_AngleVel, AngleRad, Dt, Omega);
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

	Graphics()->TextureSet(TileTex);

	Graphics()->QuadsBegin();
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
	Graphics()->QuadsEnd();
}

// Critically damped spring: smooth arrival at target without oscillation
// Omega = natural frequency (higher = faster response)
vec2 CVisuals::SpringDamp(vec2 &Pos, vec2 &Vel, vec2 Target, float Dt, float Omega)
{
	float Exp = expf(-Omega * Dt);
	vec2 Delta = Pos - Target;
	Pos = Target + (Delta + (Vel + Delta * Omega) * Dt) * Exp;
	Vel = (Vel - (Vel + Delta * Omega) * Omega * Dt) * Exp;
	return Pos;
}

float CVisuals::SpringDampAngle(float &Angle, float &Vel, float Target, float Dt, float Omega)
{
	float Exp = expf(-Omega * Dt);
	float Delta = Angle - Target;
	Angle = Target + (Delta + (Vel + Delta * Omega) * Dt) * Exp;
	Vel = (Vel - (Vel + Delta * Omega) * Omega * Dt) * Exp;
	return Angle;
}

CSpringState &CVisuals::FindOrCreateSpring(int SnapId, vec2 Pos, vec2 Pos2, float Angle)
{
	auto it = m_Springs.find(SnapId);
	if(it == m_Springs.end())
	{
		CSpringState State;
		State.m_Pos = Pos;
		State.m_Vel = vec2(0, 0);
		State.m_Pos2 = Pos2;
		State.m_Vel2 = vec2(0, 0);
		State.m_Angle = Angle;
		State.m_AngleVel = 0;
		m_Springs[SnapId] = State;
		return m_Springs[SnapId];
	}
	return it->second;
}

void CVisuals::CleanupSprings()
{
	// Erase stale entries in-place instead of rebuilding the map
	for(auto it = m_Springs.begin(); it != m_Springs.end();)
	{
		bool Found = false;
		for(const CVisualItem &Item : m_vVisualItems)
		{
			if(Item.m_Id == it->first)
			{
				Found = true;
				break;
			}
		}
		if(!Found)
			it = m_Springs.erase(it);
		else
			++it;
	}
}

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

void CVisuals::ApplyGroupScreenMapping(int GroupIndex)
{
	CLayers *pLayers = GameClient()->Layers();
	if(!pLayers || GroupIndex < 0 || GroupIndex >= pLayers->NumGroups())
		return;

	CMapItemGroup *pGroup = pLayers->GetGroup(GroupIndex);
	if(!pGroup)
		return;

	float Zoom = GameClient()->m_Camera.m_Zoom;
	vec2 Center = GameClient()->m_Camera.m_Center;

	int ParallaxZoom = std::clamp(maximum(pGroup->m_ParallaxX, pGroup->m_ParallaxY), 0, 100);
	float aPoints[4];
	Graphics()->MapScreenToWorld(Center.x, Center.y,
		pGroup->m_ParallaxX, pGroup->m_ParallaxY, (float)ParallaxZoom,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(),
		Zoom, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

IGraphics::CTextureHandle CVisuals::GetImageTexture(int ImageIndex)
{
	if(ImageIndex < 0 || ImageIndex >= GameClient()->m_MapImages.Num())
		return IGraphics::CTextureHandle();

	// Check lazy cache first
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

void CVisuals::OnMapLoad()
{
	// Clear lazy-loaded texture cache
	for(auto &[Index, Handle] : m_TexCache)
	{
		if(!Handle.IsNullTexture())
			Graphics()->UnloadTexture(&Handle);
	}
	m_TexCache.clear();
	m_Springs.clear();

	// Find game group index
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
		return;

	// Collect visual items
	m_vVisualItems.clear();
	for(const CSnapEntities &Ent : GameClient()->SnapEntities())
	{
		const IClient::CSnapItem Item = Ent.m_Item;
		if(Item.m_Type == NETOBJTYPE_DDNETVISUALLINE ||
			Item.m_Type == NETOBJTYPE_DDNETVISUALCIRCLE ||
			Item.m_Type == NETOBJTYPE_DDNETVISUALQUAD ||
			Item.m_Type == NETOBJTYPE_DDNETVISUALTILE)
		{
			CVisualItem VisItem;
			VisItem.m_Type = Item.m_Type;
			VisItem.m_Id = Item.m_Id;
			VisItem.m_pData = Item.m_pData;
			VisItem.m_pPrevData = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id);
			VisItem.m_RenderOrder = GetRenderOrderFromSnap(Item.m_Type, Item.m_pData);
			VisItem.m_Flags = GetFlagsFromSnap(Item.m_Type, Item.m_pData);
			m_vVisualItems.push_back(VisItem);
		}
	}

	if(m_vVisualItems.empty())
		return;

	// Cleanup stale spring entries
	CleanupSprings();

	// Sort by render order for correct z-ordering
	std::sort(m_vVisualItems.begin(), m_vVisualItems.end(),
		[](const CVisualItem &a, const CVisualItem &b) {
			return a.m_RenderOrder < b.m_RenderOrder;
		});

	// Save current screen mapping
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int PrevGroup = -2; // invalid sentinel
	bool PrevScreenSpace = false;
	for(const CVisualItem &Item : m_vVisualItems)
	{
		bool IsScreenSpace = (Item.m_Flags & VISUALFLAG_SCREEN_SPACE) != 0;

		if(IsScreenSpace)
		{
			if(!PrevScreenSpace)
			{
				// Switch to fixed screen coordinates (same as HUD: height=300, width=300*aspect)
				Graphics()->MapScreen(0, 0, 300.0f * Graphics()->ScreenAspect(), 300.0f);
				PrevScreenSpace = true;
				PrevGroup = -2;
			}
		}
		else
		{
			if(PrevScreenSpace)
			{
				Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
				PrevScreenSpace = false;
				PrevGroup = -2;
			}

			int GroupIndex = RenderOrderGroup(Item.m_RenderOrder);

			// Resolve game group alias
			int ResolvedGroup = GroupIndex;
			if(GroupIndex == RENDERORDER_GROUP_GAME)
				ResolvedGroup = m_GameGroupIndex;

			// Apply group's screen mapping when switching groups
			if(ResolvedGroup != PrevGroup)
			{
				if(ResolvedGroup >= 0 && ResolvedGroup != m_GameGroupIndex)
					ApplyGroupScreenMapping(ResolvedGroup);
				else
					Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
				PrevGroup = ResolvedGroup;
			}
		}

		RenderItem(Item);
	}

	// Restore screen mapping
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}
