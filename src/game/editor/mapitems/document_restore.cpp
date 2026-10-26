#include "image.h"
#include "layer_front.h"
#include "layer_game.h"
#include "layer_group.h"
#include "layer_quads.h"
#include "layer_sounds.h"
#include "layer_speedup.h"
#include "layer_switch.h"
#include "layer_tele.h"
#include "layer_tune.h"
#include "map.h"
#include "sound.h"

#include <base/log.h>

#include <engine/sound.h>

#include <game/editor/editor.h>

namespace
{
	void ReportRestoreMessage(const FErrorHandler &ErrorHandler, const char *pMessage)
	{
		log_warn("editor/history", "%s", pMessage);
		ErrorHandler(pMessage);
	}

	template<typename T>
	std::uint64_t ObjectIdAt(const std::vector<std::shared_ptr<T>> &vpObjects, int Index)
	{
		return Index >= 0 && static_cast<std::size_t>(Index) < vpObjects.size() ? vpObjects[Index]->m_Id : 0;
	}

	template<typename T>
	int ObjectIndex(const std::vector<std::shared_ptr<T>> &vpObjects, std::uint64_t Id)
	{
		return DocumentIndexById(vpObjects, Id, [](const auto &pObject) { return pObject->m_Id; });
	}

	template<typename T>
	int ElementIndex(const std::vector<T> &vElements, std::uint64_t Id)
	{
		return DocumentIndexById(vElements, Id, [](const auto &Element) { return Element.m_Id; });
	}

	CLayerSessionValues CaptureLayerSession(const CLayer &Layer)
	{
		CLayerSessionValues Session;
		Session.m_Visible = Layer.m_Visible;
		Session.m_Readonly = Layer.m_Readonly;
		if(const auto *pTiles = dynamic_cast<const CLayerTiles *>(&Layer))
		{
			str_copy(Session.m_Filename.Buffer(), pTiles->m_aFilename);
			Session.m_KnownTextModeLayer = pTiles->m_KnownTextModeLayer;
			Session.m_RenderOverlays = pTiles->m_RenderOverlays;
		}
		if(const auto *pTele = dynamic_cast<const CLayerTele *>(&Layer))
		{
			Session.m_TeleNumber = pTele->m_TeleNumber;
			Session.m_TeleCheckpointNumber = pTele->m_TeleCheckpointNumber;
			Session.m_GotoTeleOffset = pTele->m_GotoTeleOffset;
			Session.m_GotoTeleLastPos = pTele->m_GotoTeleLastPos;
		}
		if(const auto *pSpeedup = dynamic_cast<const CLayerSpeedup *>(&Layer))
		{
			Session.m_SpeedupForce = pSpeedup->m_SpeedupForce;
			Session.m_SpeedupMaxSpeed = pSpeedup->m_SpeedupMaxSpeed;
			Session.m_SpeedupAngle = pSpeedup->m_SpeedupAngle;
		}
		if(const auto *pSwitch = dynamic_cast<const CLayerSwitch *>(&Layer))
		{
			Session.m_SwitchNumber = pSwitch->m_SwitchNumber;
			Session.m_SwitchDelay = pSwitch->m_SwitchDelay;
			Session.m_GotoSwitchOffset = pSwitch->m_GotoSwitchOffset;
			Session.m_GotoSwitchLastPos = pSwitch->m_GotoSwitchLastPos;
		}
		if(const auto *pTune = dynamic_cast<const CLayerTune *>(&Layer))
		{
			Session.m_TuningNumber = pTune->m_TuningNumber;
			Session.m_GotoTuneOffset = pTune->m_GotoTuneOffset;
			Session.m_GotoTuneLastPos = pTune->m_GotoTuneLastPos;
		}
		return Session;
	}

	void ApplyLayerSession(CLayer &Layer, const CLayerSessionValues &Session)
	{
		Layer.m_Visible = Session.m_Visible;
		Layer.m_Readonly = Session.m_Readonly;
		if(auto *pTiles = dynamic_cast<CLayerTiles *>(&Layer))
		{
			str_copy(pTiles->m_aFilename, Session.m_Filename.Buffer());
			pTiles->m_KnownTextModeLayer = Session.m_KnownTextModeLayer;
			pTiles->m_RenderOverlays = Session.m_RenderOverlays;
		}
		if(auto *pTele = dynamic_cast<CLayerTele *>(&Layer))
		{
			pTele->m_TeleNumber = Session.m_TeleNumber;
			pTele->m_TeleCheckpointNumber = Session.m_TeleCheckpointNumber;
			pTele->m_GotoTeleOffset = Session.m_GotoTeleOffset;
			pTele->m_GotoTeleLastPos = Session.m_GotoTeleLastPos;
		}
		if(auto *pSpeedup = dynamic_cast<CLayerSpeedup *>(&Layer))
		{
			pSpeedup->m_SpeedupForce = Session.m_SpeedupForce;
			pSpeedup->m_SpeedupMaxSpeed = Session.m_SpeedupMaxSpeed;
			pSpeedup->m_SpeedupAngle = Session.m_SpeedupAngle;
		}
		if(auto *pSwitch = dynamic_cast<CLayerSwitch *>(&Layer))
		{
			pSwitch->m_SwitchNumber = Session.m_SwitchNumber;
			pSwitch->m_SwitchDelay = Session.m_SwitchDelay;
			pSwitch->m_GotoSwitchOffset = Session.m_GotoSwitchOffset;
			pSwitch->m_GotoSwitchLastPos = Session.m_GotoSwitchLastPos;
		}
		if(auto *pTune = dynamic_cast<CLayerTune *>(&Layer))
		{
			pTune->m_TuningNumber = Session.m_TuningNumber;
			pTune->m_GotoTuneOffset = Session.m_GotoTuneOffset;
			pTune->m_GotoTuneLastPos = Session.m_GotoTuneLastPos;
		}
	}

	class CPreparedRestore
	{
	public:
		CMapValues m_Map;
		CDocumentSessionValues m_Session;
		std::vector<std::shared_ptr<CLayerGroup>> m_vpGroups;
		std::vector<std::shared_ptr<CEditorImage>> m_vpImages;
		std::vector<std::shared_ptr<CEditorSound>> m_vpSounds;
		std::vector<std::shared_ptr<CEnvelope>> m_vpEnvelopes;
		std::shared_ptr<CLayerGroup> m_pGameGroup;
		std::shared_ptr<CLayerGame> m_pGameLayer;
		std::shared_ptr<CLayerFront> m_pFrontLayer;
		std::shared_ptr<CLayerTele> m_pTeleLayer;
		std::shared_ptr<CLayerSpeedup> m_pSpeedupLayer;
		std::shared_ptr<CLayerSwitch> m_pSwitchLayer;
		std::shared_ptr<CLayerTune> m_pTuneLayer;
		int m_SelectedGroup = 0;
		std::vector<int> m_vSelectedLayers;
		std::vector<int> m_vSelectedQuads;
		int m_SelectedEnvelope = -1;
		int m_SelectedQuadEnvelope = -1;
		int m_SelectedImage = -1;
		int m_SelectedSound = -1;
		int m_SelectedSoundSource = -1;
		std::vector<std::pair<int, int>> m_vSelectedPoints;
		std::pair<int, int> m_TangentIn{-1, -1};
		std::pair<int, int> m_TangentOut{-1, -1};
		std::vector<std::string> m_vWarnings;
	};
}

void CEditorMap::RememberDocumentSession()
{
	for(const auto &pGroup : m_vpGroups)
	{
		m_DocumentSession.m_Groups[pGroup->m_Id] = {pGroup->m_Visible, pGroup->m_Collapse};
		for(const auto &pLayer : pGroup->m_vpLayers)
			m_DocumentSession.m_Layers[pLayer->m_Id] = CaptureLayerSession(*pLayer);
	}
}

void CEditorMap::PruneDocumentSession(std::span<const CEditorDocumentValues *const> RetainedDocuments)
{
	std::unordered_set<std::uint64_t> RetainedIds;
	for(const auto &pGroup : m_vpGroups)
	{
		RetainedIds.insert(pGroup->m_Id);
		for(const auto &pLayer : pGroup->m_vpLayers)
			RetainedIds.insert(pLayer->m_Id);
	}
	for(const auto *pDocument : RetainedDocuments)
		for(const auto &Group : pDocument->m_Groups.Read())
		{
			RetainedIds.insert(Group.Read().m_Id);
			for(const auto &Layer : Group.Read().m_Layers.Read())
				RetainedIds.insert(Layer.Read().m_Id);
		}
	m_DocumentSession.Prune(RetainedIds);
}

bool CEditorMap::RestoreDocumentAtSafePoint(const CEditorDocumentValues &Document, const FErrorHandler &ErrorHandler)
{
	CPreparedRestore Prepared;
	auto Identities = m_ObjectIds;
	std::string Error;
	if(!Document.Validate(Error))
	{
		ErrorHandler(Error.c_str());
		return false;
	}
	Prepared.m_Map = static_cast<const CMapValues &>(Document);
	Prepared.m_Session = m_DocumentSession;
	for(const auto &pGroup : m_vpGroups)
	{
		Prepared.m_Session.m_Groups[pGroup->m_Id] = {pGroup->m_Visible, pGroup->m_Collapse};
		for(const auto &pLayer : pGroup->m_vpLayers)
			Prepared.m_Session.m_Layers[pLayer->m_Id] = CaptureLayerSession(*pLayer);
	}
	const auto ReserveIdentity = [&](std::uint64_t Id) {
		const bool Reserved = Identities.ReserveThrough(Id);
		dbg_assert(Reserved, "validated document identity cannot be reserved");
		(void)Reserved;
	};
	for(const auto &Image : Document.m_Images.Read())
	{
		ReserveIdentity(Image.m_Id);
		const int Previous = ObjectIndex(m_vpImages, Image.m_Id);
		if(Previous >= 0 && static_cast<const CEditorImageValues &>(*m_vpImages[Previous]) == Image)
		{
			Prepared.m_vpImages.push_back(m_vpImages[Previous]);
			continue;
		}
		auto pImage = std::make_shared<CEditorImage>(this, Image.m_Id);
		static_cast<CEditorImageValues &>(*pImage) = Image;
		pImage->m_Automapper.SetDeferredSource(pImage->m_aName);
		if(pImage->m_Content)
		{
			const int Flags = pImage->m_Width % 16 == 0 && pImage->m_Height % 16 == 0 ? Editor()->Graphics()->TextureLoadFlags() : 0;
			pImage->m_Texture = Editor()->Graphics()->LoadTextureRaw(pImage->ImageCopy(), Flags, pImage->m_aName);
			if(!pImage->m_Texture.IsValid())
				Prepared.m_vWarnings.emplace_back("Restored image content is retained, but its texture could not be created");
		}
		Prepared.m_vpImages.push_back(std::move(pImage));
	}
	for(const auto &Sound : Document.m_Sounds.Read())
	{
		ReserveIdentity(Sound.m_Id);
		const int Previous = ObjectIndex(m_vpSounds, Sound.m_Id);
		if(Previous >= 0 && static_cast<const CEditorSoundValues &>(*m_vpSounds[Previous]) == Sound)
		{
			Prepared.m_vpSounds.push_back(m_vpSounds[Previous]);
			continue;
		}
		auto pSound = std::make_shared<CEditorSound>(this, Sound.m_Id);
		static_cast<CEditorSoundValues &>(*pSound) = Sound;
		if(pSound->m_Content)
		{
			pSound->m_SoundId = Editor()->Sound()->LoadOpusFromMem(pSound->Data(), pSound->DataSize(), true, pSound->m_aName);
			if(pSound->m_SoundId < 0)
				Prepared.m_vWarnings.emplace_back("Restored sound content is retained, but its playback sample could not be created");
		}
		Prepared.m_vpSounds.push_back(std::move(pSound));
	}
	for(const auto &EnvelopeRecord : Document.m_Envelopes.Read())
	{
		const auto &Envelope = EnvelopeRecord.Read();
		ReserveIdentity(Envelope.m_Id);
		for(const auto &Point : Envelope.m_vPoints)
			ReserveIdentity(Point.m_Id);
		auto pEnvelope = std::make_shared<CEnvelope>(this, Envelope.m_Type, Envelope.m_Id);
		// The accessor is constructed for this wrapper and never copied.
		static_cast<CEnvelopeValues &>(*pEnvelope) = Envelope;
		Prepared.m_vpEnvelopes.push_back(std::move(pEnvelope));
	}
	for(const auto &GroupRecord : Document.m_Groups.Read())
	{
		const auto &Group = GroupRecord.Read();
		ReserveIdentity(Group.m_Id);
		auto pGroup = std::make_shared<CLayerGroup>(this, Group.m_Id);
		static_cast<CLayerGroupValues &>(*pGroup) = static_cast<const CLayerGroupValues &>(Group);
		const auto &GroupSession = Prepared.m_Session.m_Groups[Group.m_Id];
		pGroup->m_Visible = GroupSession.m_Visible;
		pGroup->m_Collapse = GroupSession.m_Collapse;
		if(Group.m_GameGroup)
			Prepared.m_pGameGroup = pGroup;
		for(const auto &LayerRecord : Group.m_Layers.Read())
		{
			const auto &Layer = LayerRecord.Read();
			ReserveIdentity(Layer.m_Id);
			std::shared_ptr<CLayer> pLayer;
			if(const auto *pTiles = std::get_if<CTileLayerDocumentValues>(&Layer.m_Data))
			{
				std::shared_ptr<CLayerTiles> pTileLayer;
				// Empty constructors avoid allocating a second full tile plane.
				if(pTiles->m_HasGame)
					pTileLayer = Prepared.m_pGameLayer = std::make_shared<CLayerGame>(this, 0, 0, Layer.m_Id);
				else if(pTiles->m_HasFront)
					pTileLayer = Prepared.m_pFrontLayer = std::make_shared<CLayerFront>(this, 0, 0, Layer.m_Id);
				else if(pTiles->m_HasTele)
				{
					pTileLayer = Prepared.m_pTeleLayer = std::make_shared<CLayerTele>(this, 0, 0, Layer.m_Id);
					static_cast<CLayerTeleValues &>(*Prepared.m_pTeleLayer) = std::get<CLayerTeleValues>(pTiles->m_Auxiliary);
				}
				else if(pTiles->m_HasSpeedup)
				{
					pTileLayer = Prepared.m_pSpeedupLayer = std::make_shared<CLayerSpeedup>(this, 0, 0, Layer.m_Id);
					static_cast<CLayerSpeedupValues &>(*Prepared.m_pSpeedupLayer) = std::get<CLayerSpeedupValues>(pTiles->m_Auxiliary);
				}
				else if(pTiles->m_HasSwitch)
				{
					pTileLayer = Prepared.m_pSwitchLayer = std::make_shared<CLayerSwitch>(this, 0, 0, Layer.m_Id);
					static_cast<CLayerSwitchValues &>(*Prepared.m_pSwitchLayer) = std::get<CLayerSwitchValues>(pTiles->m_Auxiliary);
				}
				else if(pTiles->m_HasTune)
				{
					pTileLayer = Prepared.m_pTuneLayer = std::make_shared<CLayerTune>(this, 0, 0, Layer.m_Id);
					static_cast<CLayerTuneValues &>(*Prepared.m_pTuneLayer) = std::get<CLayerTuneValues>(pTiles->m_Auxiliary);
				}
				else
					pTileLayer = std::make_shared<CLayerTiles>(this, 0, 0, Layer.m_Id);
				static_cast<CLayerTilesValues &>(*pTileLayer) = static_cast<const CLayerTilesValues &>(*pTiles);
				pLayer = std::move(pTileLayer);
			}
			else if(const auto *pQuads = std::get_if<CLayerQuadsValues>(&Layer.m_Data))
			{
				for(const auto &Quad : pQuads->m_vQuads)
					ReserveIdentity(Quad.m_Id);
				auto pQuadLayer = std::make_shared<CLayerQuads>(this, Layer.m_Id);
				static_cast<CLayerQuadsValues &>(*pQuadLayer) = *pQuads;
				pLayer = std::move(pQuadLayer);
			}
			else
			{
				const auto &Sounds = std::get<CLayerSoundsValues>(Layer.m_Data);
				for(const auto &Source : Sounds.m_vSources)
					ReserveIdentity(Source.m_Id);
				auto pSoundLayer = std::make_shared<CLayerSounds>(this, Layer.m_Id);
				static_cast<CLayerSoundsValues &>(*pSoundLayer) = Sounds;
				pLayer = std::move(pSoundLayer);
			}
			static_cast<CLayerValues &>(*pLayer) = static_cast<const CLayerValues &>(Layer);
			ApplyLayerSession(*pLayer, Prepared.m_Session.m_Layers[Layer.m_Id]);
			pGroup->m_vpLayers.push_back(std::move(pLayer));
		}
		Prepared.m_vpGroups.push_back(std::move(pGroup));
	}

	const auto pOldGroup = SelectedGroup();
	std::vector<std::uint64_t> vLayerIds;
	if(pOldGroup)
		for(const int Index : m_vSelectedLayers)
			vLayerIds.push_back(ObjectIdAt(pOldGroup->m_vpLayers, Index));
	int GroupIndex = ObjectIndex(Prepared.m_vpGroups, pOldGroup ? pOldGroup->m_Id : 0);
	for(const auto LayerId : vLayerIds)
	{
		for(std::size_t Index = 0; Index < Prepared.m_vpGroups.size(); ++Index)
			if(ObjectIndex(Prepared.m_vpGroups[Index]->m_vpLayers, LayerId) >= 0)
			{
				GroupIndex = static_cast<int>(Index);
				break;
			}
		if(GroupIndex >= 0 && ObjectIndex(Prepared.m_vpGroups[GroupIndex]->m_vpLayers, LayerId) >= 0)
			break;
	}
	if(GroupIndex < 0)
		GroupIndex = ObjectIndex(Prepared.m_vpGroups, Prepared.m_pGameGroup->m_Id);
	Prepared.m_SelectedGroup = GroupIndex;
	const auto &vpSelectedGroupLayers = Prepared.m_vpGroups[GroupIndex]->m_vpLayers;
	for(const auto Id : vLayerIds)
	{
		const int Index = ObjectIndex(vpSelectedGroupLayers, Id);
		if(Index >= 0)
			Prepared.m_vSelectedLayers.push_back(Index);
	}
	if(Prepared.m_vSelectedLayers.empty() && !vpSelectedGroupLayers.empty())
		Prepared.m_vSelectedLayers.push_back(0);
	const auto pOldLayer = SelectedLayer(0);
	const auto pNewLayer = Prepared.m_vSelectedLayers.empty() ? nullptr : vpSelectedGroupLayers[Prepared.m_vSelectedLayers[0]];
	if(pOldLayer && pNewLayer && pOldLayer->m_Id == pNewLayer->m_Id)
	{
		if(const auto *pOldQuads = dynamic_cast<const CLayerQuads *>(pOldLayer.get()))
			if(const auto *pNewQuads = dynamic_cast<const CLayerQuads *>(pNewLayer.get()))
				for(const int Index : m_vSelectedQuads)
					if(Index >= 0 && static_cast<std::size_t>(Index) < pOldQuads->m_vQuads.size())
					{
						const int Restored = ElementIndex(pNewQuads->m_vQuads, pOldQuads->m_vQuads[Index].m_Id);
						if(Restored >= 0)
							Prepared.m_vSelectedQuads.push_back(Restored);
					}
		if(const auto *pOldSounds = dynamic_cast<const CLayerSounds *>(pOldLayer.get()))
			if(const auto *pNewSounds = dynamic_cast<const CLayerSounds *>(pNewLayer.get()))
				if(m_SelectedSoundSource >= 0 && static_cast<std::size_t>(m_SelectedSoundSource) < pOldSounds->m_vSources.size())
					Prepared.m_SelectedSoundSource = ElementIndex(pNewSounds->m_vSources, pOldSounds->m_vSources[m_SelectedSoundSource].m_Id);
	}
	const auto EnvelopeId = ObjectIdAt(m_vpEnvelopes, m_SelectedEnvelope);
	Prepared.m_SelectedEnvelope = ObjectIndex(Prepared.m_vpEnvelopes, EnvelopeId);
	Prepared.m_SelectedQuadEnvelope = ObjectIndex(Prepared.m_vpEnvelopes, ObjectIdAt(m_vpEnvelopes, m_SelectedQuadEnvelope));
	Prepared.m_SelectedImage = ObjectIndex(Prepared.m_vpImages, ObjectIdAt(m_vpImages, m_SelectedImage));
	Prepared.m_SelectedSound = ObjectIndex(Prepared.m_vpSounds, ObjectIdAt(m_vpSounds, m_SelectedSound));
	if(Prepared.m_SelectedEnvelope >= 0)
	{
		const auto &OldPoints = m_vpEnvelopes[m_SelectedEnvelope]->m_vPoints;
		const auto &NewPoints = Prepared.m_vpEnvelopes[Prepared.m_SelectedEnvelope]->m_vPoints;
		const auto ResolvePoint = [&](std::pair<int, int> Selection) {
			if(Selection.first < 0 || static_cast<std::size_t>(Selection.first) >= OldPoints.size())
				return std::pair(-1, -1);
			const int Index = ElementIndex(NewPoints, OldPoints[Selection.first].m_Id);
			return Index < 0 ? std::pair(-1, -1) : std::pair(Index, Selection.second);
		};
		for(const auto &Selection : m_vSelectedEnvelopePoints)
		{
			const auto Restored = ResolvePoint(Selection);
			if(Restored.first >= 0)
				Prepared.m_vSelectedPoints.push_back(Restored);
		}
		Prepared.m_TangentIn = ResolvePoint(m_SelectedTangentInPoint);
		Prepared.m_TangentOut = ResolvePoint(m_SelectedTangentOutPoint);
	}
	else if(!Prepared.m_vpEnvelopes.empty())
		Prepared.m_SelectedEnvelope = 0;
	if(Prepared.m_SelectedImage < 0 && !Prepared.m_vpImages.empty())
		Prepared.m_SelectedImage = 0;
	if(Prepared.m_SelectedSound < 0 && !Prepared.m_vpSounds.empty())
		Prepared.m_SelectedSound = 0;

	// The caller has retired document callbacks. UI teardown may invoke external
	// close handlers, so finish it before publishing the document below.
	if(Editor()->Map() == this)
	{
		Editor()->Ui()->ClearObjectReferences();
		Editor()->Reset();
		Editor()->m_pUiGotContext = nullptr;
		Editor()->m_pColorPickerPopupActiveId = nullptr;
		Editor()->m_ColorPickerPopupContext.m_State = EEditState::NONE;
	}
	m_MapViewState.m_ActiveOp = CMapView::EActiveOp::NONE;
	m_FontTyperState.Reset();
	m_QuadKnifeState.Reset();
	m_ObjectIds = Identities;
	static_cast<CMapValues &>(*this) = std::move(Prepared.m_Map);
	m_DocumentSession = std::move(Prepared.m_Session);
	m_vpGroups.swap(Prepared.m_vpGroups);
	m_vpImages.swap(Prepared.m_vpImages);
	m_vpSounds.swap(Prepared.m_vpSounds);
	m_vpEnvelopes.swap(Prepared.m_vpEnvelopes);
	m_pGameGroup = std::move(Prepared.m_pGameGroup);
	m_pGameLayer = std::move(Prepared.m_pGameLayer);
	m_pFrontLayer = std::move(Prepared.m_pFrontLayer);
	m_pTeleLayer = std::move(Prepared.m_pTeleLayer);
	m_pSpeedupLayer = std::move(Prepared.m_pSpeedupLayer);
	m_pSwitchLayer = std::move(Prepared.m_pSwitchLayer);
	m_pTuneLayer = std::move(Prepared.m_pTuneLayer);
	m_SelectedGroup = Prepared.m_SelectedGroup;
	m_vSelectedLayers = std::move(Prepared.m_vSelectedLayers);
	m_vSelectedQuads = std::move(Prepared.m_vSelectedQuads);
	if(m_vSelectedQuads.empty())
		m_SelectedQuadPoints = 0;
	m_CurrentQuadIndex = -1;
	m_SelectedEnvelope = Prepared.m_SelectedEnvelope;
	m_SelectedQuadEnvelope = Prepared.m_SelectedQuadEnvelope;
	m_SelectedQuadEnvelopePoint = -1;
	m_SelectedImage = Prepared.m_SelectedImage;
	m_SelectedSound = Prepared.m_SelectedSound;
	m_SelectedSoundSource = Prepared.m_SelectedSoundSource;
	m_vSelectedEnvelopePoints = std::move(Prepared.m_vSelectedPoints);
	m_SelectedTangentInPoint = Prepared.m_TangentIn;
	m_SelectedTangentOutPoint = Prepared.m_TangentOut;
	m_UpdateEnvPointInfo = true;
	CheckIntegrity();
	for(const auto &Warning : Prepared.m_vWarnings)
		ReportRestoreMessage(ErrorHandler, Warning.c_str());
	return true;
}
