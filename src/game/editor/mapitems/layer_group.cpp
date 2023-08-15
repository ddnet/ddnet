#include "layer_group.h"

#include <game/editor/editor.h>

#include "layer.h"
#include "layer_front.h"
#include "layer_game.h"
#include "layer_speedup.h"
#include "layer_switch.h"
#include "layer_tele.h"
#include "layer_tiles.h"
#include "layer_tune.h"

void CLayerGroup::Convert(CUIRect *pRect)
{
	pRect->x += m_OffsetX;
	pRect->y += m_OffsetY;
}

void CLayerGroup::Mapping(float *pPoints)
{
	float ParallaxZoom = Editor()->m_PreviewZoom ? m_ParallaxZoom : 100.0f;

	RenderTools()->MapScreenToWorld(
		Editor()->MapView()->GetWorldOffset().x, Editor()->MapView()->GetWorldOffset().y,
		m_ParallaxX, m_ParallaxY, ParallaxZoom, m_OffsetX, m_OffsetY,
		Graphics()->ScreenAspect(), Editor()->MapView()->WorldZoom(), pPoints);

	pPoints[0] += Editor()->MapView()->GetEditorOffset().x;
	pPoints[1] += Editor()->MapView()->GetEditorOffset().y;
	pPoints[2] += Editor()->MapView()->GetEditorOffset().x;
	pPoints[3] += Editor()->MapView()->GetEditorOffset().y;
}

void CLayerGroup::MapScreen()
{
	float aPoints[4];
	Mapping(aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void CLayerGroup::Render()
{
	MapScreen();

	if(m_UseClipping)
	{
		float aPoints[4];
		Editor()->Map()->m_pGameGroup->Mapping(aPoints);
		float x0 = (m_ClipX - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y0 = (m_ClipY - aPoints[1]) / (aPoints[3] - aPoints[1]);
		float x1 = ((m_ClipX + m_ClipW) - aPoints[0]) / (aPoints[2] - aPoints[0]);
		float y1 = ((m_ClipY + m_ClipH) - aPoints[1]) / (aPoints[3] - aPoints[1]);

		Graphics()->ClipEnable((int)(x0 * Graphics()->ScreenWidth()), (int)(y0 * Graphics()->ScreenHeight()),
			(int)((x1 - x0) * Graphics()->ScreenWidth()), (int)((y1 - y0) * Graphics()->ScreenHeight()));
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
			if(Editor()->m_ShowDetail || !(pLayer->m_Flags & LAYERFLAG_DETAIL))
				pLayer->Render();
		}
	}

	for(auto &pLayer : m_vpLayers)
	{
		if(pLayer->m_Visible && pLayer->m_Type == LAYERTYPE_TILES && pLayer != Editor()->Map()->m_pGameLayer && pLayer != Editor()->Map()->m_pFrontLayer && pLayer != Editor()->Map()->m_pTeleLayer && pLayer != Editor()->Map()->m_pSpeedupLayer && pLayer != Editor()->Map()->m_pSwitchLayer && pLayer != Editor()->Map()->m_pTuneLayer)
		{
			std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			if(pTiles->m_Game || pTiles->m_Front || pTiles->m_Tele || pTiles->m_Speedup || pTiles->m_Tune || pTiles->m_Switch)
			{
				pLayer->Render();
			}
		}
	}

	if(m_UseClipping)
		Graphics()->ClipDisable();
}

void CLayerGroup::AddLayer(const std::shared_ptr<CLayer> &pLayer)
{
	Editor()->Map()->OnModify();
	m_vpLayers.push_back(pLayer);
}

void CLayerGroup::DeleteLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;
	m_vpLayers.erase(m_vpLayers.begin() + Index);
	Editor()->Map()->OnModify();
}

void CLayerGroup::DuplicateLayer(int Index)
{
	if(Index < 0 || Index >= (int)m_vpLayers.size())
		return;

	std::shared_ptr<CLayer> pDup = m_vpLayers[Index]->Duplicate();
	m_vpLayers.insert(m_vpLayers.begin() + Index + 1, pDup);

	Editor()->Map()->OnModify();
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
	Editor()->Map()->OnModify();
	std::swap(m_vpLayers[Index0], m_vpLayers[Index1]);
	return Index1;
}

bool CLayerGroup::IsEmpty() const
{
	return m_vpLayers.empty();
}

void CLayerGroup::OnEdited()
{
	if(!m_CustomParallaxZoom)
		m_ParallaxZoom = GetParallaxZoomDefault(m_ParallaxX, m_ParallaxY);
}

void CLayerGroup::Clear()
{
	m_vpLayers.clear();
}

void CLayerGroup::ModifyImageIndex(const FIndexModifyFunction &Func)
{
	for(auto &pLayer : m_vpLayers)
		pLayer->ModifyImageIndex(Func);
}

void CLayerGroup::ModifyEnvelopeIndex(const FIndexModifyFunction &Func)
{
	for(auto &pLayer : m_vpLayers)
		pLayer->ModifyEnvelopeIndex(Func);
}

void CLayerGroup::ModifySoundIndex(const FIndexModifyFunction &Func)
{
	for(auto &pLayer : m_vpLayers)
		pLayer->ModifySoundIndex(Func);
}
