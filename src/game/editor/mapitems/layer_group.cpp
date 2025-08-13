#include "layer_group.h"

#include <base/math.h>
#include <engine/shared/config.h>
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
	float NormalParallaxZoom = std::clamp((double)(maximum(m_ParallaxX, m_ParallaxY)), 0., 100.);
	float ParallaxZoom = m_pMap->Editor()->m_PreviewZoom ? NormalParallaxZoom : 100.0f;

	m_pMap->Editor()->Graphics()->MapScreenToWorld(
		m_pMap->Editor()->MapView()->GetWorldOffset().x, m_pMap->Editor()->MapView()->GetWorldOffset().y,
		m_ParallaxX, m_ParallaxY, ParallaxZoom, m_OffsetX, m_OffsetY,
		m_pMap->Editor()->Graphics()->ScreenAspect(), m_pMap->Editor()->MapView()->GetWorldZoom(), pPoints);

	pPoints[0] += m_pMap->Editor()->MapView()->GetEditorOffset().x;
	pPoints[1] += m_pMap->Editor()->MapView()->GetEditorOffset().y;
	pPoints[2] += m_pMap->Editor()->MapView()->GetEditorOffset().x;
	pPoints[3] += m_pMap->Editor()->MapView()->GetEditorOffset().y;
}

void CLayerGroup::MapScreen() const
{
	float aPoints[4];
	Mapping(aPoints);
	m_pMap->Editor()->Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CLayerGroup::Render()
{
	MapScreen();
	IGraphics *pGraphics = m_pMap->Editor()->Graphics();

	if(m_UseClipping)
	{
		float aPoints[4];
		m_pMap->m_pGameGroup->Mapping(aPoints);
		float ScreenWidth = aPoints[2] - aPoints[0];
		float ScreenHeight = aPoints[3] - aPoints[1];
		float Left = m_ClipX - aPoints[0];
		float Top = m_ClipY - aPoints[1];
		float Right = (m_ClipX + m_ClipW) - aPoints[0];
		float Bottom = (m_ClipY + m_ClipH) - aPoints[1];

		int ClipX = (int)std::round(Left * pGraphics->ScreenWidth() / ScreenWidth);
		int ClipY = (int)std::round(Top * pGraphics->ScreenHeight() / ScreenHeight);

		pGraphics->ClipEnable(
			ClipX,
			ClipY,
			(int)std::round(Right * pGraphics->ScreenWidth() / ScreenWidth) - ClipX,
			(int)std::round(Bottom * pGraphics->ScreenHeight() / ScreenHeight) - ClipY);
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible)
		{
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

				if(g_Config.m_EdShowIngameEntities && pLayer->IsEntitiesLayer() && (pLayer == m_pMap->m_pGameLayer || pLayer == m_pMap->m_pFrontLayer || pLayer == m_pMap->m_pSwitchLayer))
				{
					if(pLayer != m_pMap->m_pSwitchLayer)
						m_pMap->Editor()->RenderGameEntities(pTiles);
					m_pMap->Editor()->RenderSwitchEntities(pTiles);
				}

				if(pTiles->m_HasGame || pTiles->m_HasFront || pTiles->m_HasTele || pTiles->m_HasSpeedup || pTiles->m_HasTune || pTiles->m_HasSwitch)
					continue;
			}
			if(m_pMap->Editor()->m_ShowDetail || !(pLayer->m_Flags & LAYERFLAG_DETAIL))
				pLayer->Render();
		}
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible && pLayer->m_Type == LAYERTYPE_TILES && !pLayer->IsEntitiesLayer())
		{
			std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			if(pTiles->m_HasGame || pTiles->m_HasFront || pTiles->m_HasTele || pTiles->m_HasSpeedup || pTiles->m_HasTune || pTiles->m_HasSwitch)
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
	*pWidth = 0.0f;
	*pHeight = 0.0f;
	for(const auto &pLayer : m_vpLayers)
	{
		float LayerWidth, LayerHeight;
		pLayer->GetSize(&LayerWidth, &LayerHeight);
		*pWidth = maximum(*pWidth, LayerWidth);
		*pHeight = maximum(*pHeight, LayerHeight);
	}
}

int CLayerGroup::MoveLayer(int IndexFrom, int IndexTo)
{
	if(IndexFrom < 0 || IndexFrom >= (int)m_vpLayers.size())
		return IndexFrom;
	if(IndexTo < 0 || IndexTo >= (int)m_vpLayers.size())
		return IndexFrom;
	if(IndexFrom == IndexTo)
		return IndexFrom;
	m_pMap->OnModify();
	auto pMovedLayer = m_vpLayers[IndexFrom];
	m_vpLayers.erase(m_vpLayers.begin() + IndexFrom);
	m_vpLayers.insert(m_vpLayers.begin() + IndexTo, pMovedLayer);
	return IndexTo;
}

bool CLayerGroup::IsEmpty() const
{
	return m_vpLayers.empty();
}

void CLayerGroup::Clear()
{
	m_vpLayers.clear();
}

void CLayerGroup::ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	for(auto &pLayer : m_vpLayers)
	{
		pLayer->ModifyImageIndex(IndexModifyFunction);
	}
}

void CLayerGroup::ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	for(auto &pLayer : m_vpLayers)
	{
		pLayer->ModifyEnvelopeIndex(IndexModifyFunction);
	}
}

void CLayerGroup::ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	for(auto &pLayer : m_vpLayers)
	{
		pLayer->ModifySoundIndex(IndexModifyFunction);
	}
}
