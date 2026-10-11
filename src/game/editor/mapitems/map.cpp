#include "map.h"

#include <base/str.h>

#include <game/editor/editor.h>
#include <game/editor/mapitems/image.h>
#include <game/editor/mapitems/layer_front.h>
#include <game/editor/mapitems/layer_game.h>
#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/editor/mapitems/layer_sounds.h>
#include <game/editor/mapitems/layer_tiles.h>
#include <game/editor/mapitems/sound.h>

CEditorMap::CEditorMap(CEditor *pEditor) :
	m_DocumentHistory(this),

	m_EnvelopeEvaluator(this),
	m_MapSettingsCommandContext(pEditor->m_MapSettingsBackend.NewContextWithInput()),
	m_pEditor(pEditor)
{
	m_aFilename[0] = '\0';
	m_ValidSaveFilename = false;
	m_CloseOnSave = false;
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

	m_SelectedGroup = 0;
	m_vSelectedLayers.clear();
	DeselectQuads();
	DeselectQuadPoints();
	m_SelectedQuadEnvelope = -1;
	m_SelectedQuadEnvelopePoint = -1;
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
	m_ShowDetail = true;
	m_PreviewZoom = false;

	m_MapViewState.Reset(Editor());
	m_MapGridState.Reset();
	m_ProofModeState.Reset();
	m_QuadKnifeState.Reset();
	m_EnvelopeEditorState.Reset(Editor());
	m_MapSettingsCommandContext.Reset();
	m_FontTyperState.Reset();
}

std::uint64_t CEditorMap::AllocateObjectId()
{
	const auto Id = m_ObjectIds.Allocate();
	if(!Id)
	{
		// The invalid sentinel is rejected by completion validation; the owner
		// rolls back the whole edit without publishing an aliased identity.
		return 0;
	}
	return *Id;
}

template<typename TUsage>
void CEditorMap::VisitLiveStorage(TUsage &Usage) const
{
	const auto AccountOwner = [&](const auto &Value) {
		if constexpr(std::is_same_v<TUsage, editor_history::CStorageObservation>)
			Usage.Observe(Value);
		else
			Value.Account(Usage);
	};
	const auto AccountVector = [&](const auto &Values) { Usage.Add(Values.data(), Values.capacity() * sizeof(typename std::decay_t<decltype(Values)>::value_type)); };
	Usage.Add(this, sizeof(CEditorMap));
	AccountVector(m_vSettings);
	AccountVector(m_vpGroups);
	AccountVector(m_vpImages);
	AccountVector(m_vpSounds);
	AccountVector(m_vpEnvelopes);
	for(const auto &pImage : m_vpImages)
	{
		Usage.Add(pImage.get(), sizeof(CEditorImage));
		if(pImage->m_Content)
			AccountOwner(*pImage->m_Content);
	}
	for(const auto &pSound : m_vpSounds)
	{
		Usage.Add(pSound.get(), sizeof(CEditorSound));
		if(pSound->m_Content)
			AccountOwner(*pSound->m_Content);
	}
	for(const auto &pEnvelope : m_vpEnvelopes)
	{
		Usage.Add(pEnvelope.get(), sizeof(CEnvelope));
		AccountVector(pEnvelope->m_vPoints);
	}
	for(const auto &pGroup : m_vpGroups)
	{
		Usage.Add(pGroup.get(), sizeof(CLayerGroup));
		AccountVector(pGroup->m_vpLayers);
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(const auto *pTiles = dynamic_cast<const CLayerTiles *>(pLayer.get()))
			{
				size_t WrapperBytes = sizeof(CLayerTiles);
				if(dynamic_cast<const CLayerGame *>(pTiles))
					WrapperBytes = sizeof(CLayerGame);
				else if(dynamic_cast<const CLayerFront *>(pTiles))
					WrapperBytes = sizeof(CLayerFront);
				else if(dynamic_cast<const CLayerTele *>(pTiles))
					WrapperBytes = sizeof(CLayerTele);
				else if(dynamic_cast<const CLayerSpeedup *>(pTiles))
					WrapperBytes = sizeof(CLayerSpeedup);
				else if(dynamic_cast<const CLayerSwitch *>(pTiles))
					WrapperBytes = sizeof(CLayerSwitch);
				else if(dynamic_cast<const CLayerTune *>(pTiles))
					WrapperBytes = sizeof(CLayerTune);
				Usage.Add(pTiles, WrapperBytes);
				AccountOwner(pTiles->m_Tiles);
				if(const auto *pTele = dynamic_cast<const CLayerTele *>(pTiles))
					AccountOwner(pTele->m_TeleTiles);
				if(const auto *pSpeedup = dynamic_cast<const CLayerSpeedup *>(pTiles))
					AccountOwner(pSpeedup->m_SpeedupTiles);
				if(const auto *pSwitch = dynamic_cast<const CLayerSwitch *>(pTiles))
					AccountOwner(pSwitch->m_SwitchTiles);
				if(const auto *pTune = dynamic_cast<const CLayerTune *>(pTiles))
					AccountOwner(pTune->m_TuneTiles);
			}
			else if(const auto *pQuads = dynamic_cast<const CLayerQuads *>(pLayer.get()))
			{
				Usage.Add(pQuads, sizeof(CLayerQuads));
				AccountVector(pQuads->m_vQuads);
			}
			else if(const auto *pSounds = dynamic_cast<const CLayerSounds *>(pLayer.get()))
			{
				Usage.Add(pSounds, sizeof(CLayerSounds));
				AccountVector(pSounds->m_vSources);
			}
		}
	}
}

void CEditorMap::AccountLiveStorage(editor_history::CStorageUsage &Usage) const
{
	VisitLiveStorage(Usage);
}

void CEditorMap::ObserveLiveStorage(editor_history::CStorageObservation &Observation) const
{
	VisitLiveStorage(Observation);
}

void CEditorMap::ObserveRuntimeCaches(editor_history::CStorageObservation &Observation) const
{
	m_FingerprintCache.Observe(Observation);
	for(const auto &pImage : m_vpImages)
		Observation.Add(&pImage->m_Automapper, pImage->m_Automapper.StorageBytes());
}

void CEditorMap::AccountRuntimeCaches(editor_history::CStorageUsage &Usage) const
{
	m_FingerprintCache.Account(Usage);
	for(const auto &pImage : m_vpImages)
		pImage->m_Automapper.Account(Usage);
}

void CEditorMap::OnModify()
{
	if(m_DocumentHistory.Ready())
		m_DocumentHistory.RefreshPresentation();
	else
	{
		m_Modified = true;
		m_ModifiedAuto = true;
	}
	m_LastModifiedTime = Editor()->Client()->GlobalTime();
	// Stop scheduled map closing if the map was modified
	m_CloseOnSave = false;
}

void CEditorMap::ResetModifiedState()
{
	m_Modified = false;
	m_ModifiedAuto = false;
	m_LastModifiedTime = -1.0f;
	m_LastSaveTime = Editor()->Client()->GlobalTime();
}

std::shared_ptr<const CEditorDocumentValues> CEditorMap::CaptureSavedDocument(std::string &Error)
{
	auto Document = CaptureDocument(m_pLastCapturedDocument.get(), Error);
	if(!Document)
		return nullptr;
	m_pLastCapturedDocument = std::make_shared<const CEditorDocumentValues>(std::move(*Document));
	return m_pLastCapturedDocument;
}

bool CEditorMap::InitializeLoadedSaveState()
{
	if(!m_DocumentHistory.Initialize())
		return false;
	if(!m_pSaveState->InitializeLoaded(m_DocumentHistory.History()->Revisions().front().m_PersistedKey))
		return false;
	m_DocumentHistory.RefreshPresentation();
	return true;
}

bool CEditorMap::RefreshSavedState()
{
	m_DocumentHistory.RefreshPresentation();
	return m_DocumentHistory.Ready();
}

bool CEditorMap::CompleteSave(const CEditorSaveState::CTicket &Ticket, bool Success)
{
	if(!OwnsSave(Ticket))
		return false;
	if(!Success)
	{
		if(Ticket.Kind() == editor_history::ESaveKind::MANUAL)
			m_CloseOnSave = false;
		return false;
	}
	if(!m_pSaveState->Complete(Ticket, true))
		return false;
	if(Ticket.Kind() == editor_history::ESaveKind::MANUAL)
	{
		str_copy(m_aFilename, Ticket.m_Destination.c_str());
		m_ValidSaveFilename = true;
	}
	RefreshSavedState();
	return true;
}

void CEditorMap::CreateDefault()
{
	// Add default background group, quad layer and quad
	std::shared_ptr<CLayerGroup> pGroup = NewGroup();
	pGroup->m_ParallaxX = 0;
	pGroup->m_ParallaxY = 0;
	std::shared_ptr<CLayerQuads> pLayer = std::make_shared<CLayerQuads>(this);
	CQuadValues *pQuad = pLayer->NewQuad(0, 0, 1600, 1200);
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
	if(!m_DocumentHistory.Initialize())
		Editor()->ShowFileDialogError("Could not initialize document history.");
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

int CEditorMap::ImageIndex(CDocumentReference Reference) const
{
	return DocumentIndexById(m_vpImages, Reference.m_Id, [](const auto &pObject) { return pObject->m_Id; });
}

CDocumentReference CEditorMap::ImageReference(int Index) const
{
	return {Index >= 0 && static_cast<std::size_t>(Index) < m_vpImages.size() ? m_vpImages[Index]->m_Id : 0};
}

int CEditorMap::SoundIndex(CDocumentReference Reference) const
{
	return DocumentIndexById(m_vpSounds, Reference.m_Id, [](const auto &pObject) { return pObject->m_Id; });
}

CDocumentReference CEditorMap::SoundReference(int Index) const
{
	return {Index >= 0 && static_cast<std::size_t>(Index) < m_vpSounds.size() ? m_vpSounds[Index]->m_Id : 0};
}

int CEditorMap::EnvelopeIndex(CDocumentReference Reference) const
{
	return DocumentIndexById(m_vpEnvelopes, Reference.m_Id, [](const auto &pObject) { return pObject->m_Id; });
}

CDocumentReference CEditorMap::EnvelopeReference(int Index) const
{
	return {Index >= 0 && static_cast<std::size_t>(Index) < m_vpEnvelopes.size() ? m_vpEnvelopes[Index]->m_Id : 0};
}

void CEditorMap::VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	OnModify();
	for(auto &pGroup : m_vpGroups)
	{
		pGroup->VisitImageReferences(ReferenceFunction);
	}
}

void CEditorMap::VisitAllEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	OnModify();
	for(auto &pGroup : m_vpGroups)
	{
		pGroup->VisitEnvelopeReferences(ReferenceFunction);
	}
}

void CEditorMap::VisitSoundReferences(const FDocumentReferenceFunction &ReferenceFunction)
{
	OnModify();
	for(auto &pGroup : m_vpGroups)
	{
		pGroup->VisitSoundReferences(ReferenceFunction);
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
	m_QuadKnifeState.Reset();
}

void CEditorMap::SelectNextLayer()
{
	int CurrentLayer = 0;
	for(const auto &Selected : m_vSelectedLayers)
		CurrentLayer = std::max(Selected, CurrentLayer);
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
		CurrentLayer = std::min(Selected, CurrentLayer);
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

std::vector<CQuadValues *> CEditorMap::SelectedQuads()
{
	std::shared_ptr<CLayerQuads> pQuadLayer = std::static_pointer_cast<CLayerQuads>(SelectedLayerType(0, LAYERTYPE_QUADS));
	std::vector<CQuadValues *> vpQuads;
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
	if(!pLayer || m_vSelectedQuads.empty() || m_vSelectedLayers.size() != 1)
		return;

	m_DocumentHistory.Edit(this, "Delete quads", editor_history::ECategory::MAP, [&] {
		auto vIndices = m_vSelectedQuads;
		std::sort(vIndices.begin(), vIndices.end(), std::greater<>());
		for(const int Index : vIndices)
			pLayer->m_vQuads.erase(pLayer->m_vQuads.begin() + Index);
		DeselectQuads();
		OnModify();
	});
}

std::shared_ptr<CEnvelope> CEditorMap::NewEnvelope(CEnvelope::EType Type)
{
	OnModify();
	std::shared_ptr<CEnvelope> pEnvelope = std::make_shared<CEnvelope>(this, Type);
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

void CEditorMap::DeleteEnvelope(int Index)
{
	if(Index < 0 || Index >= static_cast<int>(m_vpEnvelopes.size()))
		return;
	const auto Reference = EnvelopeReference(Index);
	VisitAllEnvelopeReferences([&](CDocumentReference &Element) {
		if(Element == Reference)
			Element = {};
	});
	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + Index);
	OnModify();
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

	auto pMovedEnvelope = m_vpEnvelopes[IndexFrom];
	m_vpEnvelopes.erase(m_vpEnvelopes.begin() + IndexFrom);
	m_vpEnvelopes.insert(m_vpEnvelopes.begin() + IndexTo, pMovedEnvelope);

	return IndexTo;
}

bool CEditorMap::IsEnvelopeUsed(int EnvelopeIndex) const
{
	for(const auto &pGroup : m_vpGroups)
	{
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			if(pLayer->IsEnvelopeUsed(EnvelopeIndex))
			{
				return true;
			}
		}
	}
	return false;
}

void CEditorMap::RemoveUnusedEnvelopes()
{
	m_DocumentHistory.Edit(this, "Remove unused envelopes", editor_history::ECategory::ENVELOPE, [&] {
		for(size_t Index = 0; Index < m_vpEnvelopes.size();)
		{
			if(IsEnvelopeUsed(Index))
				++Index;
			else
				DeleteEnvelope(Index);
		}
	});
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
			if(pLayer->IsImageUsed(ImageIndex))
			{
				return true;
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
			if(pLayer->IsSoundUsed(SoundIndex))
			{
				return true;
			}
		}
	}
	return false;
}

CSoundSourceValues *CEditorMap::SelectedSoundSource() const
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

	if(!pT)
		return;
	m_DocumentHistory.Edit(this, "Make borders", editor_history::ECategory::MAP, [&] {
		for(int i = 0; i < pT->Width() * pT->Height(); ++i)
		{
			if(i % pT->Width() < 2 || i % pT->Width() > pT->Width() - 3 || i < pT->Width() * 2 || i > pT->Width() * (pT->Height() - 2))
			{
				int x = i % pT->Width();
				int y = i / pT->Width();

				CTile Current = pT->m_Tiles[i];
				Current.m_Index = 1;
				pT->SetTile(x, y, Current);
			}
		}

		OnModify();
	});
}
