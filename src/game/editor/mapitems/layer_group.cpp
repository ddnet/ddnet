#include "layer_group.h"

#include <base/math.h>
#include <game/editor/editor.h>

CLayerGroup::CLayerGroup()
{
	m_vpLayers.clear();
	m_aName[0] = 0;
	m_Visible = true;
	m_Collapse = false;
	m_GameGroup = false;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_ParallaxX = 100;
	m_ParallaxY = 100;

	m_UseClipping = 0;
	m_ClipX = 0;
	m_ClipY = 0;
	m_ClipW = 0;
	m_ClipH = 0;
}

CLayerGroup::~CLayerGroup()
{
	m_vpLayers.clear();
}

void CLayerGroup::Convert(CUIRect *pRect) const
{
	pRect->x += m_OffsetX;
	pRect->y += m_OffsetY;
}

void CLayerGroup::Mapping(float *pPoints) const
{
	float NormalParallaxZoom = clamp((double)(maximum(m_ParallaxX, m_ParallaxY)), 0., 100.);
	float ParallaxZoom = m_pMap->m_pEditor->m_PreviewZoom ? NormalParallaxZoom : 100.0f;

	m_pMap->m_pEditor->RenderTools()->MapScreenToWorld(
		m_pMap->m_pEditor->MapView()->GetWorldOffset().x, m_pMap->m_pEditor->MapView()->GetWorldOffset().y,
		m_ParallaxX, m_ParallaxY, ParallaxZoom, m_OffsetX, m_OffsetY,
		m_pMap->m_pEditor->Graphics()->ScreenAspect(), m_pMap->m_pEditor->MapView()->GetWorldZoom(), pPoints);

	pPoints[0] += m_pMap->m_pEditor->MapView()->GetEditorOffset().x;
	pPoints[1] += m_pMap->m_pEditor->MapView()->GetEditorOffset().y;
	pPoints[2] += m_pMap->m_pEditor->MapView()->GetEditorOffset().x;
	pPoints[3] += m_pMap->m_pEditor->MapView()->GetEditorOffset().y;
}

void CLayerGroup::MapScreen() const
{
	float aPoints[4];
	Mapping(aPoints);
	m_pMap->m_pEditor->Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CLayerGroup::Render()
{
	MapScreen();
	IGraphics *pGraphics = m_pMap->m_pEditor->Graphics();

	if(m_UseClipping)
	{
		float aPoints[4];
		m_pMap->m_pGameGroup->Mapping(aPoints);
		float x0 = (m_ClipX - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y0 = (m_ClipY - aPoints[1]) / (aPoints[3] - aPoints[1]);
		float x1 = ((m_ClipX + m_ClipW) - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y1 = ((m_ClipY + m_ClipH) - aPoints[1]) / (aPoints[3] - aPoints[1]);

		pGraphics->ClipEnable((int)(x0 * pGraphics->ScreenWidth()), (int)(y0 * pGraphics->ScreenHeight()),
			(int)((x1 - x0) * pGraphics->ScreenWidth()), (int)((y1 - y0) * pGraphics->ScreenHeight()));
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible)
		{
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				if(pTiles->m_Game || pTiles->m_Front || pTiles->m_Tele || pTiles->m_Speedup || pTiles->m_Tune || pTiles->m_Switch)
					continue;
			}
			if(m_pMap->m_pEditor->m_ShowDetail || !(pLayer->m_Flags & LAYERFLAG_DETAIL))
				pLayer->Render();
		}
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible && pLayer->m_Type == LAYERTYPE_TILES && !pLayer->IsEntitiesLayer())
		{
			std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			if(pTiles->m_Game || pTiles->m_Front || pTiles->m_Tele || pTiles->m_Speedup || pTiles->m_Tune || pTiles->m_Switch)
			{
				pLayer->Render();
			}
		}
	}

	if(m_UseClipping)
		pGraphics->ClipDisable();
}

void CLayerGroup::AddLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pMap->OnModify();
	m_vpLayers.push_back(pLayer);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;
	m_vpLayers.erase(m_vpLayers.begin() + Index);
	m_pMap->OnModify();
}

void CLayerGroup::DuplicateLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;

	std::shared_ptr<CLayer> pDup = m_vpLayers[Index]->Duplicate();
	m_vpLayers.insert(m_vpLayers.begin() + Index + 1, pDup);

	m_pMap->OnModify();
}

void CLayerGroup::GetSize(float *pWidth, float *pHeight) const
{
	*pWidth = 0;
	*pHeight = 0;
	for(const auto &pLayer : m_vpLayers)
	{
		float lw, lh;
		pLayer->GetSize(&lw, &lh);
		*pWidth = maximum(*pWidth, lw);
		*pHeight = maximum(*pHeight, lh);
	}
}

int CLayerGroup::SwapLayers(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= (int)m_vpLayers.size())
		return Index0;
	if(Index1 < 0 || Index1 >= (int)m_vpLayers.size())
		return Index0;
	if(Index0 == Index1)
		return Index0;
	m_pMap->OnModify();
	std::swap(m_vpLayers[Index0], m_vpLayers[Index1]);
	return Index1;
}
