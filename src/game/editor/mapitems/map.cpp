#include "map.h"

#include <base/system.h>

#include <game/editor/editor.h>
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
	m_pEditor->m_SelectedEnvelope = MoveEnvelope((int)m_vpEnvelopes.size() - 1, Index);
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

	m_SelectedImage = 0;
	m_SelectedSound = 0;

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

void CEditorMap::MakeGameLayer(const std::shared_ptr<CLayer> &pLayer)
{
	m_pGameLayer = std::static_pointer_cast<CLayerGame>(pLayer);
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
