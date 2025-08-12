#include "map.h"

#include <base/system.h>

#include <game/editor/editor.h>
#include <game/editor/mapitems/layer_front.h>
#include <game/editor/mapitems/layer_game.h>
#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/editor/mapitems/layer_sounds.h>
#include <game/editor/mapitems/layer_tiles.h>

void CEditorMap::CMapInfo::Reset()
{
	m_aAuthor[0] = '\0';
	m_aVersion[0] = '\0';
	m_aCredits[0] = '\0';
	m_aLicense[0] = '\0';
}

void CEditorMap::CMapInfo::Copy(const CMapInfo &Source)
{
	str_copy(m_aAuthor, Source.m_aAuthor);
	str_copy(m_aVersion, Source.m_aVersion);
	str_copy(m_aCredits, Source.m_aCredits);
	str_copy(m_aLicense, Source.m_aLicense);
}

void CEditorMap::OnModify()
{
	m_Modified = true;
	m_ModifiedAuto = true;
	m_LastModifiedTime = m_pEditor->Client()->GlobalTime();
}

std::shared_ptr<CEnvelope> CEditorMap::NewEnvelope(CEnvelope::EType Type)
{
	OnModify();
	std::shared_ptr<CEnvelope> pEnv = std::make_shared<CEnvelope>(Type);
	m_vpEnvelopes.push_back(pEnv);
	return pEnv;
}

void CEditorMap::InsertEnvelope(int Index, std::shared_ptr<CEnvelope> &pEnvelope)
{
	if(Index < 0 || Index >= (int)m_vpEnvelopes.size() + 1)
		return;
	m_pEditor->m_Map.m_vpEnvelopes.push_back(pEnvelope);
	m_pEditor->m_SelectedEnvelope = m_pEditor->m_Map.MoveEnvelope((int)m_pEditor->m_Map.m_vpEnvelopes.size() - 1, Index);
}

void CEditorMap::UpdateEnvelopeReferences(int Index, std::shared_ptr<CEnvelope> &pEnvelope, std::vector<std::shared_ptr<IEditorEnvelopeReference>> &vpEditorObjectReferences)
{
	// update unrestored quad and soundsource references
	for(auto &pEditorObjRef : vpEditorObjectReferences)
		pEditorObjRef->SetEnvelope(pEnvelope, Index);
}

std::vector<std::shared_ptr<IEditorEnvelopeReference>> CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= (int)m_vpEnvelopes.size())
		return std::vector<std::shared_ptr<IEditorEnvelopeReference>>();

	OnModify();

	std::vector<std::shared_ptr<IEditorEnvelopeReference>> vpEditorObjectReferences = VisitEnvelopeReferences([Index](int &ElementIndex) {
		if(ElementIndex == Index)
		{
			ElementIndex = -1;
			return true;
		}
		else if(ElementIndex > Index)
			ElementIndex--;
		return false;
	});

	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + Index);
	return vpEditorObjectReferences;
}

int CEditorMap::MoveEnvelope(int IndexFrom, int IndexTo)
{
	if(IndexFrom < 0 || IndexFrom >= (int)m_vpEnvelopes.size())
		return IndexFrom;
	if(IndexTo < 0 || IndexTo >= (int)m_vpEnvelopes.size())
		return IndexFrom;
	if(IndexFrom == IndexTo)
		return IndexFrom;

	OnModify();

	VisitEnvelopeReferences([IndexFrom, IndexTo](int &ElementIndex) {
		if(ElementIndex == IndexFrom)
			ElementIndex = IndexTo;
		else if(IndexFrom < IndexTo && ElementIndex > IndexFrom && ElementIndex <= IndexTo)
			ElementIndex--;
		else if(IndexTo < IndexFrom && ElementIndex < IndexFrom && ElementIndex >= IndexTo)
			ElementIndex++;
		return false;
	});

	auto pMovedEnvelope = m_vpEnvelopes[IndexFrom];
	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + IndexFrom);
	m_vpEnvelopes.insert(m_vpEnvelopes.begin() + IndexTo, pMovedEnvelope);

	return IndexTo;
}

template<typename F>
std::vector<std::shared_ptr<IEditorEnvelopeReference>> CEditorMap::VisitEnvelopeReferences(F &&Visitor)
{
	std::vector<std::shared_ptr<IEditorEnvelopeReference>> vpUpdatedReferences;
	for(auto &pGroup : m_vpGroups)
	{
		for(auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(pLayer);
				std::shared_ptr<CLayerQuadsEnvelopeReference> pQuadLayerReference = std::make_shared<CLayerQuadsEnvelopeReference>(pLayerQuads);
				for(int QuadId = 0; QuadId < (int)pLayerQuads->m_vQuads.size(); ++QuadId)
				{
					auto &Quad = pLayerQuads->m_vQuads[QuadId];
					if(Visitor(Quad.m_PosEnv))
						pQuadLayerReference->AddQuadIndex(QuadId);
					if(Visitor(Quad.m_ColorEnv))
						pQuadLayerReference->AddQuadIndex(QuadId);
				}
				if(!pQuadLayerReference->Empty())
					vpUpdatedReferences.push_back(pQuadLayerReference);
			}
			else if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				std::shared_ptr<CLayerTilesEnvelopeReference> pTileLayerReference = std::make_shared<CLayerTilesEnvelopeReference>(pLayerTiles);
				if(Visitor(pLayerTiles->m_ColorEnv))
					vpUpdatedReferences.push_back(pTileLayerReference);
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
				std::shared_ptr<CLayerSoundEnvelopeReference> pSoundLayerReference = std::make_shared<CLayerSoundEnvelopeReference>(pLayerSounds);

				for(int SourceId = 0; SourceId < (int)pLayerSounds->m_vSources.size(); ++SourceId)
				{
					auto &Source = pLayerSounds->m_vSources[SourceId];
					if(Visitor(Source.m_PosEnv))
						pSoundLayerReference->AddSoundSourceIndex(SourceId);
					if(Visitor(Source.m_SoundEnv))
						pSoundLayerReference->AddSoundSourceIndex(SourceId);
				}
				if(!pSoundLayerReference->Empty())
					vpUpdatedReferences.push_back(pSoundLayerReference);
			}
		}
	}
	return vpUpdatedReferences;
}

std::shared_ptr<CLayerGroup> CEditorMap::NewGroup()
{
	OnModify();
	std::shared_ptr<CLayerGroup> pGroup = std::make_shared<CLayerGroup>();
	pGroup->m_pMap = this;
	m_vpGroups.push_back(pGroup);
	return pGroup;
}

int CEditorMap::MoveGroup(int IndexFrom, int IndexTo)
{
	if(IndexFrom < 0 || IndexFrom >= (int)m_vpGroups.size())
		return IndexFrom;
	if(IndexTo < 0 || IndexTo >= (int)m_vpGroups.size())
		return IndexFrom;
	if(IndexFrom == IndexTo)
		return IndexFrom;
	OnModify();
	auto pMovedGroup = m_vpGroups[IndexFrom];
	m_vpGroups.erase(m_vpGroups.begin() + IndexFrom);
	m_vpGroups.insert(m_vpGroups.begin() + IndexTo, pMovedGroup);
	return IndexTo;
}

void CEditorMap::DeleteGroup(int Index)
{
	if(Index < 0 || Index >= (int)m_vpGroups.size())
		return;
	OnModify();
	m_vpGroups.erase(m_vpGroups.begin() + Index);
}

void CEditorMap::ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	OnModify();
	for(auto &pGroup : m_vpGroups)
	{
		pGroup->ModifyImageIndex(IndexModifyFunction);
	}
}

void CEditorMap::ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	OnModify();
	for(auto &pGroup : m_vpGroups)
	{
		pGroup->ModifyEnvelopeIndex(IndexModifyFunction);
	}
}

void CEditorMap::ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction)
{
	OnModify();
	for(auto &pGroup : m_vpGroups)
	{
		pGroup->ModifySoundIndex(IndexModifyFunction);
	}
}

void CEditorMap::Clean()
{
	m_vpGroups.clear();
	m_vpEnvelopes.clear();
	m_vpImages.clear();
	m_vpSounds.clear();
	m_vSettings.clear();

	m_pGameGroup = nullptr;
	m_pGameLayer = nullptr;
	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pFrontLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;

	m_MapInfo.Reset();
	m_MapInfoTmp.Reset();

	m_Modified = false;
	m_ModifiedAuto = false;
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
