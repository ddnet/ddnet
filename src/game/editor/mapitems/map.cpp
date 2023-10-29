#include <game/editor/editor.h>

#include "image.h"

void CEditorMap::OnModify()
{
	m_Modified = true;
	m_ModifiedAuto = true;
	m_LastModifiedTime = m_pEditor->Client()->GlobalTime();
}

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= (int)m_vpEnvelopes.size())
		return;

	OnModify();

	VisitEnvelopeReferences([Index](int &ElementIndex) {
		if(ElementIndex == Index)
			ElementIndex = -1;
		else if(ElementIndex > Index)
			ElementIndex--;
	});

	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + Index);
}

void CEditorMap::SwapEnvelopes(int Index0, int Index1)
{
	if(Index0 < 0 || Index0 >= (int)m_vpEnvelopes.size())
		return;
	if(Index1 < 0 || Index1 >= (int)m_vpEnvelopes.size())
		return;
	if(Index0 == Index1)
		return;

	OnModify();

	VisitEnvelopeReferences([Index0, Index1](int &ElementIndex) {
		if(ElementIndex == Index0)
			ElementIndex = Index1;
		else if(ElementIndex == Index1)
			ElementIndex = Index0;
	});

	std::swap(m_vpEnvelopes[Index0], m_vpEnvelopes[Index1]);
}

template<typename F>
void CEditorMap::VisitEnvelopeReferences(F &&Visitor)
{
	for(auto &pGroup : m_vpGroups)
	{
		for(auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(pLayer);
				for(auto &Quad : pLayerQuads->m_vQuads)
				{
					Visitor(Quad.m_PosEnv);
					Visitor(Quad.m_ColorEnv);
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				Visitor(pLayerTiles->m_ColorEnv);
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
				for(auto &Source : pLayerSounds->m_vSources)
				{
					Visitor(Source.m_PosEnv);
					Visitor(Source.m_SoundEnv);
				}
			}
		}
	}
}

void CEditorMap::MakeGameLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pGameLayer = std::static_pointer_cast<CLayerGame>(pLayer);
	m_pGameLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup)
{
	m_pGameGroup = std::move(pGroup);
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game");
}

void CEditorMap::Clean()
{
	m_vpGroups.clear();
	m_vpEnvelopes.clear();
	m_vpImages.clear();
	m_vpSounds.clear();

	m_MapInfo.Reset();
	m_MapInfoTmp.Reset();

	m_vSettings.clear();

	m_pGameLayer = nullptr;
	m_pGameGroup = nullptr;

	m_Modified = false;
	m_ModifiedAuto = false;

	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pFrontLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;
}

void CEditorMap::CreateDefault(IGraphics::CTextureHandle EntitiesTexture)
{
	// add background
	std::shared_ptr<CLayerGroup> pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	std::shared_ptr<CLayerQuads> pLayer = std::make_shared<CLayerQuads>(m_pEditor);
	CQuad *pQuad = pLayer->NewQuad(0, 0, 1600, 1200);
	pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = 94;
	pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = 132;
	pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = 174;
	pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = 204;
	pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = 232;
	pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = 255;
	pGroup->AddLayer(pLayer);

	// add game layer and reset front, tele, speedup, tune and switch layer pointers
	MakeGameGroup(NewGroup());
	MakeGameLayer(std::make_shared<CLayerGame>(m_pEditor, 50, 50));
	m_pGameGroup->AddLayer(m_pGameLayer);

	m_pFrontLayer = nullptr;
	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;
}

void CEditorMap::MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pTeleLayer = std::static_pointer_cast<CLayerTele>(pLayer);
	m_pTeleLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pSpeedupLayer = std::static_pointer_cast<CLayerSpeedup>(pLayer);
	m_pSpeedupLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pFrontLayer = std::static_pointer_cast<CLayerFront>(pLayer);
	m_pFrontLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(pLayer);
	m_pSwitchLayer->m_pEditor = m_pEditor;
}

void CEditorMap::MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pTuneLayer = std::static_pointer_cast<CLayerTune>(pLayer);
	m_pTuneLayer->m_pEditor = m_pEditor;
}
