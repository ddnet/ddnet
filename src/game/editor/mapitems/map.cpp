#include "map.h"

#include <base/system.h>

#include <game/editor/editor.h>
#include <game/editor/editor_actions.h>
#include <game/editor/mapitems/image.h>
#include <game/editor/mapitems/layer_front.h>
#include <game/editor/mapitems/layer_game.h>
#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/editor/mapitems/layer_sounds.h>
#include <game/editor/mapitems/layer_tiles.h>
#include <game/editor/mapitems/sound.h>
#include <game/editor/references.h>

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
	m_LastModifiedTime = Editor()->Client()->GlobalTime();
}

void CEditorMap::ResetModifiedState()
{
	m_Modified = false;
	m_ModifiedAuto = false;
	m_LastModifiedTime = -1.0f;
	m_LastSaveTime = Editor()->Client()->GlobalTime();
}

void CEditorMap::Clean()
{
	m_aFilename[0] = '\0';
	m_ValidSaveFilename = false;
	ResetModifiedState();

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

	m_EditorHistory.Clear();
	m_EnvelopeEditorHistory.Clear();
	m_ServerSettingsHistory.Clear();
	m_EnvOpTracker.Reset();

	m_SelectedGroup = 0;
	m_vSelectedLayers.clear();
	DeselectQuads();
	DeselectQuadPoints();
	m_SelectedQuadEnvelope = -1;
	m_CurrentQuadIndex = -1;
	m_SelectedEnvelope = 0;
	m_UpdateEnvPointInfo = false;
	m_vSelectedEnvelopePoints.clear();
	m_SelectedTangentInPoint = std::pair(-1, -1);
	m_SelectedTangentOutPoint = std::pair(-1, -1);
	m_SelectedImage = 0;
	m_SelectedSound = 0;
	m_SelectedSoundSource = -1;

	m_ShiftBy = 1;

	m_QuadKnife.m_Active = false;
	m_QuadKnife.m_Count = 0;
	m_QuadKnife.m_SelectedQuadIndex = -1;
	std::fill(std::begin(m_QuadKnife.m_aPoints), std::end(m_QuadKnife.m_aPoints), vec2(0.0f, 0.0f));
}

void CEditorMap::CreateDefault()
{
	// Add default background group, quad layer and quad
	std::shared_ptr<CLayerGroup> pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	std::shared_ptr<CLayerQuads> pLayer = std::make_shared<CLayerQuads>(this);
	CQuad *pQuad = pLayer->NewQuad(0, 0, 1600, 1200);
	pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = 94;
	pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = 132;
	pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = 174;
	pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = 204;
	pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = 232;
	pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = 255;
	pGroup->AddLayer(pLayer);

	// Add game group and layer
	MakeGameGroup(NewGroup());
	MakeGameLayer(std::make_shared<CLayerGame>(this, 50, 50));
	m_pGameGroup->AddLayer(m_pGameLayer);

	ResetModifiedState();
	CheckIntegrity();
	SelectGameLayer();
}

void CEditorMap::CheckIntegrity()
{
	const auto &&CheckObjectInMap = [&](const CMapObject *pMapObject, const char *pName) {
		dbg_assert(pMapObject != nullptr, "%s missing in map", pName);
		dbg_assert(pMapObject->Map() == this, "%s does not belong to map (object_map=%p, this_map=%p)", pName, pMapObject->Map(), this);
	};
	bool GameGroupMissing = true;
	CheckObjectInMap(m_pGameGroup.get(), "Game group");
	dbg_assert(m_pGameGroup->m_GameGroup, "Game group not marked as such");
	bool GameLayerMissing = true;
	CheckObjectInMap(m_pGameLayer.get(), "Game layer");
	dbg_assert(m_pGameLayer->m_HasGame, "Game layer not marked as such");
	bool FrontLayerMissing = false;
	if(m_pFrontLayer != nullptr)
	{
		FrontLayerMissing = true;
		CheckObjectInMap(m_pFrontLayer.get(), "Front layer");
		dbg_assert(m_pFrontLayer->m_HasFront, "Front layer not marked as such");
	}
	bool TeleLayerMissing = false;
	if(m_pTeleLayer != nullptr)
	{
		TeleLayerMissing = true;
		CheckObjectInMap(m_pTeleLayer.get(), "Tele layer");
		dbg_assert(m_pTeleLayer->m_HasTele, "Tele layer not marked as such");
	}
	bool SpeedupLayerMissing = false;
	if(m_pSpeedupLayer != nullptr)
	{
		SpeedupLayerMissing = true;
		CheckObjectInMap(m_pSpeedupLayer.get(), "Speedup layer");
		dbg_assert(m_pSpeedupLayer->m_HasSpeedup, "Speedup layer not marked as such");
	}
	bool SwitchLayerMissing = false;
	if(m_pSwitchLayer != nullptr)
	{
		SwitchLayerMissing = true;
		CheckObjectInMap(m_pSwitchLayer.get(), "Switch layer");
		dbg_assert(m_pSwitchLayer->m_HasSwitch, "Switch layer not marked as such");
	}
	bool TuneLayerMissing = false;
	if(m_pTuneLayer != nullptr)
	{
		TuneLayerMissing = true;
		CheckObjectInMap(m_pTuneLayer.get(), "Tune layer");
		dbg_assert(m_pTuneLayer->m_HasTune, "Tune layer not marked as such");
	}
	for(const auto &pGroup : m_vpGroups)
	{
		CheckObjectInMap(pGroup.get(), "Group");
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			CheckObjectInMap(pLayer.get(), "Layer");
		}
		if(pGroup == m_pGameGroup)
		{
			GameGroupMissing = false;
			for(const auto &pLayer : pGroup->m_vpLayers)
			{
				if(pLayer == m_pGameLayer)
				{
					GameLayerMissing = false;
				}
				if(pLayer == m_pFrontLayer)
				{
					FrontLayerMissing = false;
				}
				if(pLayer == m_pTeleLayer)
				{
					TeleLayerMissing = false;
				}
				if(pLayer == m_pSpeedupLayer)
				{
					SpeedupLayerMissing = false;
				}
				if(pLayer == m_pSwitchLayer)
				{
					SwitchLayerMissing = false;
				}
				if(pLayer == m_pTuneLayer)
				{
					TuneLayerMissing = false;
				}
			}
			dbg_assert(!GameLayerMissing, "Game layer missing in game group");
			dbg_assert(!FrontLayerMissing, "Front layer missing in game group");
			dbg_assert(!TeleLayerMissing, "Tele layer missing in game group");
			dbg_assert(!SpeedupLayerMissing, "Speedup layer missing in game group");
			dbg_assert(!SwitchLayerMissing, "Switch layer missing in game group");
			dbg_assert(!TuneLayerMissing, "Tune layer missing in game group");
		}
	}
	dbg_assert(!GameGroupMissing, "Game group missing in list of groups");
	for(const auto &pImage : m_vpImages)
	{
		CheckObjectInMap(pImage.get(), "Image");
	}
	for(const auto &pSound : m_vpSounds)
	{
		CheckObjectInMap(pSound.get(), "Sound");
	}
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

std::shared_ptr<CLayerGroup> CEditorMap::SelectedGroup() const
{
	if(m_SelectedGroup >= 0 && m_SelectedGroup < (int)m_vpGroups.size())
		return m_vpGroups[m_SelectedGroup];
	return nullptr;
}

std::shared_ptr<CLayerGroup> CEditorMap::NewGroup()
{
	OnModify();
	std::shared_ptr<CLayerGroup> pGroup = std::make_shared<CLayerGroup>(this);
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

void CEditorMap::MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup)
{
	m_pGameGroup = std::move(pGroup);
	m_pGameGroup->m_GameGroup = true;
	str_copy(m_pGameGroup->m_aName, "Game");
}

std::shared_ptr<CLayer> CEditorMap::SelectedLayer(int Index) const
{
	std::shared_ptr<CLayerGroup> pGroup = SelectedGroup();
	if(!pGroup)
		return nullptr;

	if(Index < 0 || Index >= (int)m_vSelectedLayers.size())
		return nullptr;

	int LayerIndex = m_vSelectedLayers[Index];

	if(LayerIndex >= 0 && LayerIndex < (int)m_vpGroups[m_SelectedGroup]->m_vpLayers.size())
		return pGroup->m_vpLayers[LayerIndex];
	return nullptr;
}

std::shared_ptr<CLayer> CEditorMap::SelectedLayerType(int Index, int Type) const
{
	std::shared_ptr<CLayer> pLayer = SelectedLayer(Index);
	if(pLayer && pLayer->m_Type == Type)
		return pLayer;
	return nullptr;
}

void CEditorMap::SelectLayer(int LayerIndex, int GroupIndex)
{
	if(GroupIndex != -1)
		m_SelectedGroup = GroupIndex;

	m_vSelectedLayers.clear();
	DeselectQuads();
	DeselectQuadPoints();
	AddSelectedLayer(LayerIndex);
}

void CEditorMap::AddSelectedLayer(int LayerIndex)
{
	m_vSelectedLayers.push_back(LayerIndex);
	m_QuadKnife.m_Active = false;
}

void CEditorMap::SelectNextLayer()
{
	int CurrentLayer = 0;
	for(const auto &Selected : m_vSelectedLayers)
		CurrentLayer = maximum(Selected, CurrentLayer);
	SelectLayer(CurrentLayer);

	if(m_vSelectedLayers[0] < (int)m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1)
	{
		SelectLayer(m_vSelectedLayers[0] + 1);
	}
	else
	{
		for(size_t Group = m_SelectedGroup + 1; Group < m_vpGroups.size(); Group++)
		{
			if(!m_vpGroups[Group]->m_vpLayers.empty())
			{
				SelectLayer(0, Group);
				break;
			}
		}
	}
}

void CEditorMap::SelectPreviousLayer()
{
	int CurrentLayer = std::numeric_limits<int>::max();
	for(const auto &Selected : m_vSelectedLayers)
		CurrentLayer = minimum(Selected, CurrentLayer);
	SelectLayer(CurrentLayer);

	if(m_vSelectedLayers[0] > 0)
	{
		SelectLayer(m_vSelectedLayers[0] - 1);
	}
	else
	{
		for(int Group = m_SelectedGroup - 1; Group >= 0; Group--)
		{
			if(!m_vpGroups[Group]->m_vpLayers.empty())
			{
				SelectLayer(m_vpGroups[Group]->m_vpLayers.size() - 1, Group);
				break;
			}
		}
	}
}

void CEditorMap::SelectGameLayer()
{
	for(size_t g = 0; g < m_vpGroups.size(); g++)
	{
		for(size_t i = 0; i < m_vpGroups[g]->m_vpLayers.size(); i++)
		{
			if(m_vpGroups[g]->m_vpLayers[i] == m_pGameLayer)
			{
				SelectLayer(i, g);
				return;
			}
		}
	}
}

void CEditorMap::MakeGameLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pGameLayer = std::static_pointer_cast<CLayerGame>(pLayer);
}

void CEditorMap::MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pTeleLayer = std::static_pointer_cast<CLayerTele>(pLayer);
}

void CEditorMap::MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pSpeedupLayer = std::static_pointer_cast<CLayerSpeedup>(pLayer);
}

void CEditorMap::MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pFrontLayer = std::static_pointer_cast<CLayerFront>(pLayer);
}

void CEditorMap::MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pSwitchLayer = std::static_pointer_cast<CLayerSwitch>(pLayer);
}

void CEditorMap::MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pTuneLayer = std::static_pointer_cast<CLayerTune>(pLayer);
}

std::vector<CQuad *> CEditorMap::SelectedQuads()
{
	std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(SelectedLayerType(0, LAYERTYPE_QUADS));
	std::vector<CQuad *> vpQuads;
	if(!pQuadLayer)
		return vpQuads;
	vpQuads.reserve(m_vSelectedQuads.size());
	for(const auto &SelectedQuad : m_vSelectedQuads)
	{
		if(SelectedQuad >= (int)pQuadLayer->m_vQuads.size())
			continue;
		vpQuads.push_back(&pQuadLayer->m_vQuads[SelectedQuad]);
	}
	return vpQuads;
}

bool CEditorMap::IsQuadSelected(int Index) const
{
	return FindSelectedQuadIndex(Index) >= 0;
}

int CEditorMap::FindSelectedQuadIndex(int Index) const
{
	for(size_t i = 0; i < m_vSelectedQuads.size(); ++i)
		if(m_vSelectedQuads[i] == Index)
			return i;
	return -1;
}

void CEditorMap::SelectQuad(int Index)
{
	m_vSelectedQuads.clear();
	m_vSelectedQuads.push_back(Index);
}

void CEditorMap::ToggleSelectQuad(int Index)
{
	int ListIndex = FindSelectedQuadIndex(Index);
	if(ListIndex < 0)
		m_vSelectedQuads.push_back(Index);
	else
		m_vSelectedQuads.erase(m_vSelectedQuads.begin() + ListIndex);
}

void CEditorMap::DeselectQuads()
{
	m_vSelectedQuads.clear();
}

bool CEditorMap::IsQuadCornerSelected(int Index) const
{
	return m_SelectedQuadPoints & (1 << Index);
}

bool CEditorMap::IsQuadPointSelected(int QuadIndex, int Index) const
{
	return IsQuadSelected(QuadIndex) && IsQuadCornerSelected(Index);
}

void CEditorMap::SelectQuadPoint(int QuadIndex, int Index)
{
	SelectQuad(QuadIndex);
	m_SelectedQuadPoints = 1 << Index;
}

void CEditorMap::ToggleSelectQuadPoint(int QuadIndex, int Index)
{
	if(IsQuadPointSelected(QuadIndex, Index))
	{
		m_SelectedQuadPoints ^= 1 << Index;
	}
	else
	{
		if(!IsQuadSelected(QuadIndex))
		{
			ToggleSelectQuad(QuadIndex);
		}

		if(!(m_SelectedQuadPoints & 1 << Index))
		{
			m_SelectedQuadPoints ^= 1 << Index;
		}
	}
}

void CEditorMap::DeselectQuadPoints()
{
	m_SelectedQuadPoints = 0;
}

void CEditorMap::DeleteSelectedQuads()
{
	std::shared_ptr<CLayerQuads> pLayer = std::static_pointer_cast<CLayerQuads>(SelectedLayerType(0, LAYERTYPE_QUADS));
	if(!pLayer)
		return;

	std::vector<int> vSelectedQuads(m_vSelectedQuads);
	std::vector<CQuad> vDeletedQuads;
	vDeletedQuads.reserve(m_vSelectedQuads.size());
	for(int i = 0; i < (int)m_vSelectedQuads.size(); ++i)
	{
		auto const &Quad = pLayer->m_vQuads[m_vSelectedQuads[i]];
		vDeletedQuads.push_back(Quad);

		pLayer->m_vQuads.erase(pLayer->m_vQuads.begin() + m_vSelectedQuads[i]);
		for(int j = i + 1; j < (int)m_vSelectedQuads.size(); ++j)
			if(m_vSelectedQuads[j] > m_vSelectedQuads[i])
				m_vSelectedQuads[j]--;

		m_vSelectedQuads.erase(m_vSelectedQuads.begin() + i);
		i--;
	}

	m_EditorHistory.RecordAction(std::make_shared<CEditorActionDeleteQuad>(this, m_SelectedGroup, m_vSelectedLayers[0], vSelectedQuads, vDeletedQuads));
}

std::shared_ptr<CEnvelope> CEditorMap::NewEnvelope(CEnvelope::EType Type)
{
	OnModify();
	std::shared_ptr<CEnvelope> pEnvelope = std::make_shared<CEnvelope>(Type);
	if(Type == CEnvelope::EType::COLOR)
	{
		pEnvelope->AddPoint(CFixedTime::FromSeconds(0.0f), {f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f)});
		pEnvelope->AddPoint(CFixedTime::FromSeconds(1.0f), {f2fx(1.0f), f2fx(1.0f), f2fx(1.0f), f2fx(1.0f)});
	}
	else
	{
		pEnvelope->AddPoint(CFixedTime::FromSeconds(0.0f), {0, 0, 0, 0});
		pEnvelope->AddPoint(CFixedTime::FromSeconds(1.0f), {0, 0, 0, 0});
	}
	m_vpEnvelopes.push_back(pEnvelope);
	return pEnvelope;
}

void CEditorMap::InsertEnvelope(int Index, std::shared_ptr<CEnvelope> &pEnvelope)
{
	if(Index < 0 || Index >= (int)m_vpEnvelopes.size() + 1)
		return;
	m_vpEnvelopes.push_back(pEnvelope);
	m_SelectedEnvelope = MoveEnvelope((int)m_vpEnvelopes.size() - 1, Index);
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

bool CEditorMap::IsEnvelopeUsed(int EnvelopeIndex) const
{
	for(const auto &pGroup : m_vpGroups)
	{
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				std::shared_ptr<CLayerQuads> pLayerQuads = std::static_pointer_cast<CLayerQuads>(pLayer);
				for(const auto &Quad : pLayerQuads->m_vQuads)
				{
					if(Quad.m_PosEnv == EnvelopeIndex || Quad.m_ColorEnv == EnvelopeIndex)
					{
						return true;
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				std::shared_ptr<CLayerSounds> pLayerSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
				for(const auto &Source : pLayerSounds->m_vSources)
				{
					if(Source.m_PosEnv == EnvelopeIndex || Source.m_SoundEnv == EnvelopeIndex)
					{
						return true;
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				std::shared_ptr<CLayerTiles> pLayerTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				if(pLayerTiles->m_ColorEnv == EnvelopeIndex)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CEditorMap::RemoveUnusedEnvelopes()
{
	m_EnvelopeEditorHistory.BeginBulk();
	int DeletedCount = 0;
	for(size_t EnvelopeIndex = 0; EnvelopeIndex < m_vpEnvelopes.size();)
	{
		if(IsEnvelopeUsed(EnvelopeIndex))
		{
			++EnvelopeIndex;
		}
		else
		{
			// deleting removes the shared ptr from the map
			std::shared_ptr<CEnvelope> pEnvelope = m_vpEnvelopes[EnvelopeIndex];
			auto vpObjectReferences = DeleteEnvelope(EnvelopeIndex);
			m_EnvelopeEditorHistory.RecordAction(std::make_shared<CEditorActionEnvelopeDelete>(this, EnvelopeIndex, vpObjectReferences, pEnvelope));
			DeletedCount++;
		}
	}
	char aDisplay[256];
	str_format(aDisplay, sizeof(aDisplay), "Tool 'Remove unused envelopes': delete %d envelopes", DeletedCount);
	m_EnvelopeEditorHistory.EndBulk(aDisplay);
}

int CEditorMap::FindEnvPointIndex(int Index, int Channel) const
{
	auto Iter = std::find(
		m_vSelectedEnvelopePoints.begin(),
		m_vSelectedEnvelopePoints.end(),
		std::pair(Index, Channel));

	if(Iter != m_vSelectedEnvelopePoints.end())
		return Iter - m_vSelectedEnvelopePoints.begin();
	else
		return -1;
}

void CEditorMap::SelectEnvPoint(int Index)
{
	m_vSelectedEnvelopePoints.clear();

	for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
		m_vSelectedEnvelopePoints.emplace_back(Index, c);
}

void CEditorMap::SelectEnvPoint(int Index, int Channel)
{
	DeselectEnvPoints();
	m_vSelectedEnvelopePoints.emplace_back(Index, Channel);
}

void CEditorMap::ToggleEnvPoint(int Index, int Channel)
{
	if(IsTangentSelected())
		DeselectEnvPoints();

	int ListIndex = FindEnvPointIndex(Index, Channel);

	if(ListIndex >= 0)
	{
		m_vSelectedEnvelopePoints.erase(m_vSelectedEnvelopePoints.begin() + ListIndex);
	}
	else
		m_vSelectedEnvelopePoints.emplace_back(Index, Channel);
}

bool CEditorMap::IsEnvPointSelected(int Index, int Channel) const
{
	int ListIndex = FindEnvPointIndex(Index, Channel);

	return ListIndex >= 0;
}

bool CEditorMap::IsEnvPointSelected(int Index) const
{
	auto Iter = std::find_if(
		m_vSelectedEnvelopePoints.begin(),
		m_vSelectedEnvelopePoints.end(),
		[&](const auto &Pair) { return Pair.first == Index; });

	return Iter != m_vSelectedEnvelopePoints.end();
}

void CEditorMap::DeselectEnvPoints()
{
	m_vSelectedEnvelopePoints.clear();
	m_SelectedTangentInPoint = std::pair(-1, -1);
	m_SelectedTangentOutPoint = std::pair(-1, -1);
}

bool CEditorMap::IsTangentSelected() const
{
	return IsTangentInSelected() || IsTangentOutSelected();
}

bool CEditorMap::IsTangentOutPointSelected(int Index, int Channel) const
{
	return m_SelectedTangentOutPoint == std::pair(Index, Channel);
}

bool CEditorMap::IsTangentOutSelected() const
{
	return m_SelectedTangentOutPoint != std::pair(-1, -1);
}

void CEditorMap::SelectTangentOutPoint(int Index, int Channel)
{
	DeselectEnvPoints();
	m_SelectedTangentOutPoint = std::pair(Index, Channel);
}

bool CEditorMap::IsTangentInPointSelected(int Index, int Channel) const
{
	return m_SelectedTangentInPoint == std::pair(Index, Channel);
}

bool CEditorMap::IsTangentInSelected() const
{
	return m_SelectedTangentInPoint != std::pair(-1, -1);
}

void CEditorMap::SelectTangentInPoint(int Index, int Channel)
{
	DeselectEnvPoints();
	m_SelectedTangentInPoint = std::pair(Index, Channel);
}

std::pair<CFixedTime, int> CEditorMap::SelectedEnvelopeTimeAndValue() const
{
	if(m_SelectedEnvelope < 0 || m_SelectedEnvelope >= (int)m_vpEnvelopes.size())
		return {};

	std::shared_ptr<CEnvelope> pEnvelope = m_vpEnvelopes[m_SelectedEnvelope];
	CFixedTime CurrentTime;
	int CurrentValue;
	if(IsTangentInSelected())
	{
		auto [SelectedIndex, SelectedChannel] = m_SelectedTangentInPoint;
		CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaX[SelectedChannel];
		CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aInTangentDeltaY[SelectedChannel];
	}
	else if(IsTangentOutSelected())
	{
		auto [SelectedIndex, SelectedChannel] = m_SelectedTangentOutPoint;
		CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaX[SelectedChannel];
		CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel] + pEnvelope->m_vPoints[SelectedIndex].m_Bezier.m_aOutTangentDeltaY[SelectedChannel];
	}
	else
	{
		auto [SelectedIndex, SelectedChannel] = m_vSelectedEnvelopePoints.front();
		CurrentTime = pEnvelope->m_vPoints[SelectedIndex].m_Time;
		CurrentValue = pEnvelope->m_vPoints[SelectedIndex].m_aValues[SelectedChannel];
	}

	return std::pair<CFixedTime, int>{CurrentTime, CurrentValue};
}

std::shared_ptr<CEditorImage> CEditorMap::SelectedImage() const
{
	if(m_SelectedImage < 0 || (size_t)m_SelectedImage >= m_vpImages.size())
	{
		return nullptr;
	}
	return m_vpImages[m_SelectedImage];
}

void CEditorMap::SelectImage(const std::shared_ptr<CEditorImage> &pImage)
{
	for(size_t i = 0; i < m_vpImages.size(); ++i)
	{
		if(m_vpImages[i] == pImage)
		{
			m_SelectedImage = i;
			break;
		}
	}
}

void CEditorMap::SelectNextImage()
{
	const int OldImage = m_SelectedImage;
	m_SelectedImage = std::clamp(m_SelectedImage, 0, (int)m_vpImages.size() - 1);
	for(size_t i = m_SelectedImage + 1; i < m_vpImages.size(); i++)
	{
		if(m_vpImages[i]->m_External == m_vpImages[m_SelectedImage]->m_External)
		{
			m_SelectedImage = i;
			break;
		}
	}
	if(m_SelectedImage == OldImage && !m_vpImages[m_SelectedImage]->m_External)
	{
		for(size_t i = 0; i < m_vpImages.size(); i++)
		{
			if(m_vpImages[i]->m_External)
			{
				m_SelectedImage = i;
				break;
			}
		}
	}
}

void CEditorMap::SelectPreviousImage()
{
	const int OldImage = m_SelectedImage;
	m_SelectedImage = std::clamp(m_SelectedImage, 0, (int)m_vpImages.size() - 1);
	for(int i = m_SelectedImage - 1; i >= 0; i--)
	{
		if(m_vpImages[i]->m_External == m_vpImages[m_SelectedImage]->m_External)
		{
			m_SelectedImage = i;
			break;
		}
	}
	if(m_SelectedImage == OldImage && m_vpImages[m_SelectedImage]->m_External)
	{
		for(int i = (int)m_vpImages.size() - 1; i >= 0; i--)
		{
			if(!m_vpImages[i]->m_External)
			{
				m_SelectedImage = i;
				break;
			}
		}
	}
}

bool CEditorMap::IsImageUsed(int ImageIndex) const
{
	for(const auto &pGroup : m_vpGroups)
	{
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				const std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
				if(pTiles->m_Image == ImageIndex)
				{
					return true;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				const std::shared_ptr<CLayerQuads> pQuads = std::static_pointer_cast<CLayerQuads>(pLayer);
				if(pQuads->m_Image == ImageIndex)
				{
					return true;
				}
			}
		}
	}
	return false;
}

std::vector<int> CEditorMap::SortImages()
{
	static const auto &&s_ImageNameComparator = [](const std::shared_ptr<CEditorImage> &pLhs, const std::shared_ptr<CEditorImage> &pRhs) {
		return str_comp(pLhs->m_aName, pRhs->m_aName) < 0;
	};
	if(std::is_sorted(m_vpImages.begin(), m_vpImages.end(), s_ImageNameComparator))
	{
		return std::vector<int>();
	}

	const std::vector<std::shared_ptr<CEditorImage>> vpTemp = m_vpImages;
	std::vector<int> vSortedIndex;
	vSortedIndex.resize(vpTemp.size());

	std::sort(m_vpImages.begin(), m_vpImages.end(), s_ImageNameComparator);
	for(size_t OldIndex = 0; OldIndex < vpTemp.size(); OldIndex++)
	{
		for(size_t NewIndex = 0; NewIndex < m_vpImages.size(); NewIndex++)
		{
			if(vpTemp[OldIndex] == m_vpImages[NewIndex])
			{
				vSortedIndex[OldIndex] = NewIndex;
				break;
			}
		}
	}
	ModifyImageIndex([vSortedIndex](int *pIndex) {
		if(*pIndex >= 0)
		{
			*pIndex = vSortedIndex[*pIndex];
		}
	});

	return vSortedIndex;
}

std::shared_ptr<CEditorSound> CEditorMap::SelectedSound() const
{
	if(m_SelectedSound < 0 || (size_t)m_SelectedSound >= m_vpSounds.size())
	{
		return nullptr;
	}
	return m_vpSounds[m_SelectedSound];
}

void CEditorMap::SelectSound(const std::shared_ptr<CEditorSound> &pSound)
{
	for(size_t i = 0; i < m_vpSounds.size(); ++i)
	{
		if(m_vpSounds[i] == pSound)
		{
			m_SelectedSound = i;
			break;
		}
	}
}

void CEditorMap::SelectNextSound()
{
	m_SelectedSound = (m_SelectedSound + 1) % m_vpSounds.size();
}

void CEditorMap::SelectPreviousSound()
{
	m_SelectedSound = (m_SelectedSound + m_vpSounds.size() - 1) % m_vpSounds.size();
}

bool CEditorMap::IsSoundUsed(int SoundIndex) const
{
	for(const auto &pGroup : m_vpGroups)
	{
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->m_Type == LAYERTYPE_SOUNDS)
			{
				std::shared_ptr<CLayerSounds> pSounds = std::static_pointer_cast<CLayerSounds>(pLayer);
				if(pSounds->m_Sound == SoundIndex)
				{
					return true;
				}
			}
		}
	}
	return false;
}

CSoundSource *CEditorMap::SelectedSoundSource() const
{
	std::shared_ptr<CLayerSounds> pSounds = std::static_pointer_cast<CLayerSounds>(SelectedLayerType(0, LAYERTYPE_SOUNDS));
	if(!pSounds)
		return nullptr;
	if(m_SelectedSoundSource >= 0 && m_SelectedSoundSource < (int)pSounds->m_vSources.size())
		return &pSounds->m_vSources[m_SelectedSoundSource];
	return nullptr;
}

void CEditorMap::PlaceBorderTiles()
{
	std::shared_ptr<CLayerTiles> pT = std::static_pointer_cast<CLayerTiles>(SelectedLayerType(0, LAYERTYPE_TILES));

	for(int i = 0; i < pT->m_Width * pT->m_Height; ++i)
	{
		if(i % pT->m_Width < 2 || i % pT->m_Width > pT->m_Width - 3 || i < pT->m_Width * 2 || i > pT->m_Width * (pT->m_Height - 2))
		{
			int x = i % pT->m_Width;
			int y = i / pT->m_Width;

			CTile Current = pT->m_pTiles[i];
			Current.m_Index = 1;
			pT->SetTile(x, y, Current);
		}
	}

	int GameGroupIndex = std::find(m_vpGroups.begin(), m_vpGroups.end(), m_pGameGroup) - m_vpGroups.begin();
	m_EditorHistory.RecordAction(std::make_shared<CEditorBrushDrawAction>(this, GameGroupIndex), "Tool 'Make borders'");

	OnModify();
}
