#include "visuals.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <generated/protocol.h>
#include <game/mapitems.h>
#include <game/visual_primitives.h>

#include <base/math.h>
#include <base/vmath.h>

#include <game/layers.h>

#include <algorithm>

void CVisuals::SetColorFromPacked(int Color)
{
	float r = UnpackVisualColorR(Color) / 255.0f;
	float g = UnpackVisualColorG(Color) / 255.0f;
	float b = UnpackVisualColorB(Color) / 255.0f;
	float a = UnpackVisualColorA(Color) / 255.0f;
	Graphics()->SetColor(r, g, b, a);
}

void CVisuals::RenderLine(const void *pData)
{
	const CNetObj_DDNetVisualLine *pLine = static_cast<const CNetObj_DDNetVisualLine *>(pData);

	vec2 From(pLine->m_FromX, pLine->m_FromY);
	vec2 To(pLine->m_ToX, pLine->m_ToY);
	int Width = pLine->m_Width;

	SetColorFromPacked(pLine->m_Color);

	if(Width <= 1)
	{
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
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

void CVisuals::RenderCircle(const void *pData)
{
	const CNetObj_DDNetVisualCircle *pCircle = static_cast<const CNetObj_DDNetVisualCircle *>(pData);

	float CenterX = (float)pCircle->m_X;
	float CenterY = (float)pCircle->m_Y;
	float Radius = (float)pCircle->m_Radius;
	bool Filled = UnpackShapeFilled(pCircle->m_Flags);
	int Segments = maximum(16, (int)(Radius / 4.0f));

	SetColorFromPacked(pCircle->m_Color);
	Graphics()->TextureClear();

	if(Filled)
	{
		Graphics()->QuadsBegin();
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

void CVisuals::RenderQuad(const void *pData)
{
	const CNetObj_DDNetVisualQuad *pQuad = static_cast<const CNetObj_DDNetVisualQuad *>(pData);

	float CenterX = (float)pQuad->m_X;
	float CenterY = (float)pQuad->m_Y;
	float HalfW = pQuad->m_W / 2.0f;
	float HalfH = pQuad->m_H / 2.0f;
	float AngleRad = (pQuad->m_Angle / 256.0f) * (pi / 180.0f);

	SetColorFromPacked(pQuad->m_Color);

	bool HasTexture = pQuad->m_ImageIndex >= 0 && pQuad->m_ImageIndex < GameClient()->m_MapImages.Num();
	bool Filled = UnpackShapeFilled(pQuad->m_Flags) || HasTexture;

	if(HasTexture)
		Graphics()->TextureSet(GameClient()->m_MapImages.Get(pQuad->m_ImageIndex));
	else
		Graphics()->TextureClear();

	if(Filled)
	{
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(AngleRad);

		IGraphics::CQuadItem QuadItem(CenterX - HalfW, CenterY - HalfH, (float)pQuad->m_W, (float)pQuad->m_H);
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

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
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

void CVisuals::RenderTile(const void *pData)
{
	const CNetObj_DDNetVisualTile *pTile = static_cast<const CNetObj_DDNetVisualTile *>(pData);

	if(pTile->m_ImageIndex < 0 || pTile->m_ImageIndex >= GameClient()->m_MapImages.Num())
		return;
	if(pTile->m_TileIndex < 0 || pTile->m_TileIndex > 255)
		return;

	float CenterX = (float)pTile->m_X;
	float CenterY = (float)pTile->m_Y;
	float AngleRad = (pTile->m_Angle / 256.0f) * (pi / 180.0f);

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

	SetColorFromPacked(pTile->m_Color);
	Graphics()->TextureSet(GameClient()->m_MapImages.Get(pTile->m_ImageIndex));

	Graphics()->QuadsBegin();
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
		RenderLine(Item.m_pData);
	else if(Item.m_Type == NETOBJTYPE_DDNETVISUALCIRCLE)
		RenderCircle(Item.m_pData);
	else if(Item.m_Type == NETOBJTYPE_DDNETVISUALQUAD)
		RenderQuad(Item.m_pData);
	else if(Item.m_Type == NETOBJTYPE_DDNETVISUALTILE)
		RenderTile(Item.m_pData);
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

void CVisuals::OnMapLoad()
{
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
			VisItem.m_pData = Item.m_pData;
			VisItem.m_RenderOrder = GetRenderOrderFromSnap(Item.m_Type, Item.m_pData);
			m_vVisualItems.push_back(VisItem);
		}
	}

	if(m_vVisualItems.empty())
		return;

	// Sort by render order for correct z-ordering
	std::sort(m_vVisualItems.begin(), m_vVisualItems.end(),
		[](const CVisualItem &a, const CVisualItem &b) {
			return a.m_RenderOrder < b.m_RenderOrder;
		});

	// Save current screen mapping
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int PrevGroup = -2; // invalid sentinel
	for(const CVisualItem &Item : m_vVisualItems)
	{
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

		RenderItem(Item);
	}

	// Restore screen mapping
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}
