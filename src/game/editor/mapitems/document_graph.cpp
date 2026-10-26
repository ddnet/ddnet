#include "document_graph.h"

#include "image.h"
#include "layer_group.h"
#include "layer_quads.h"
#include "layer_sounds.h"
#include "layer_speedup.h"
#include "layer_switch.h"
#include "layer_tele.h"
#include "layer_tune.h"
#include "map.h"
#include "sound.h"

#include <unordered_map>

namespace
{
	template<typename T>
	editor_history::CSharedValue<T> ShareRecord(T Value, const editor_history::CSharedValue<T> *pPrevious)
	{
		if(pPrevious != nullptr && pPrevious->Read() == Value)
			return *pPrevious;
		return editor_history::CSharedValue<T>(std::move(Value));
	}

	template<typename T>
	editor_history::CSharedVector<T> ShareVector(std::vector<T> vValues, const editor_history::CSharedVector<T> *pPrevious)
	{
		if(pPrevious != nullptr && pPrevious->Read() == vValues)
			return *pPrevious;
		return editor_history::CSharedVector<T>(std::move(vValues));
	}
}

std::optional<CEditorDocumentValues> CEditorMap::CaptureDocument(const CEditorDocumentValues *pPrevious, std::string &Error) const
{
	using namespace editor_history;
	CEditorDocumentValues Result;
	static_cast<CMapValues &>(Result) = static_cast<const CMapValues &>(*this);
	std::unordered_map<std::uint64_t, const CSharedValue<CGroupDocumentValues> *> PreviousGroups;
	std::unordered_map<std::uint64_t, const CSharedValue<CLayerDocumentValues> *> PreviousLayers;
	std::unordered_map<std::uint64_t, const CSharedValue<CEnvelopeValues> *> PreviousEnvelopes;
	if(pPrevious != nullptr)
	{
		for(const auto &Group : pPrevious->m_Groups.Read())
		{
			PreviousGroups.emplace(Group.Read().m_Id, &Group);
			for(const auto &Layer : Group.Read().m_Layers.Read())
				PreviousLayers.emplace(Layer.Read().m_Id, &Layer);
		}
		for(const auto &Envelope : pPrevious->m_Envelopes.Read())
			PreviousEnvelopes.emplace(Envelope.Read().m_Id, &Envelope);
	}
	std::vector<CEditorImageValues> vImages;
	vImages.reserve(m_vpImages.size());
	for(const auto &pImage : m_vpImages)
		vImages.push_back(*pImage);
	Result.m_Images = ShareVector(std::move(vImages), pPrevious != nullptr ? &pPrevious->m_Images : nullptr);
	std::vector<CEditorSoundValues> vSounds;
	vSounds.reserve(m_vpSounds.size());
	for(const auto &pSound : m_vpSounds)
		vSounds.push_back(*pSound);
	Result.m_Sounds = ShareVector(std::move(vSounds), pPrevious != nullptr ? &pPrevious->m_Sounds : nullptr);
	std::vector<CSharedValue<CEnvelopeValues>> vEnvelopes;
	vEnvelopes.reserve(m_vpEnvelopes.size());
	for(const auto &pEnvelope : m_vpEnvelopes)
		vEnvelopes.push_back(ShareRecord<CEnvelopeValues>(*pEnvelope, PreviousEnvelopes[pEnvelope->m_Id]));
	Result.m_Envelopes = ShareVector(std::move(vEnvelopes), pPrevious != nullptr ? &pPrevious->m_Envelopes : nullptr);
	std::vector<CSharedValue<CGroupDocumentValues>> vGroups;
	vGroups.reserve(m_vpGroups.size());
	for(const auto &pGroup : m_vpGroups)
	{
		CGroupDocumentValues Group;
		static_cast<CLayerGroupValues &>(Group) = static_cast<const CLayerGroupValues &>(*pGroup);
		std::vector<CSharedValue<CLayerDocumentValues>> vLayers;
		vLayers.reserve(pGroup->m_vpLayers.size());
		for(const auto &pLayer : pGroup->m_vpLayers)
		{
			CLayerDocumentValues Layer;
			static_cast<CLayerValues &>(Layer) = static_cast<const CLayerValues &>(*pLayer);
			if(const auto *pTiles = dynamic_cast<const CLayerTiles *>(pLayer.get()))
			{
				CTileLayerDocumentValues Tiles;
				static_cast<CLayerTilesValues &>(Tiles) = static_cast<const CLayerTilesValues &>(*pTiles);
				if(const auto *pTele = dynamic_cast<const CLayerTele *>(pTiles))
					Tiles.m_Auxiliary = static_cast<const CLayerTeleValues &>(*pTele);
				else if(const auto *pSpeedup = dynamic_cast<const CLayerSpeedup *>(pTiles))
					Tiles.m_Auxiliary = static_cast<const CLayerSpeedupValues &>(*pSpeedup);
				else if(const auto *pSwitch = dynamic_cast<const CLayerSwitch *>(pTiles))
					Tiles.m_Auxiliary = static_cast<const CLayerSwitchValues &>(*pSwitch);
				else if(const auto *pTune = dynamic_cast<const CLayerTune *>(pTiles))
					Tiles.m_Auxiliary = static_cast<const CLayerTuneValues &>(*pTune);
				Layer.m_Data = std::move(Tiles);
			}
			else if(const auto *pQuads = dynamic_cast<const CLayerQuads *>(pLayer.get()))
				Layer.m_Data = static_cast<const CLayerQuadsValues &>(*pQuads);
			else if(const auto *pSounds = dynamic_cast<const CLayerSounds *>(pLayer.get()))
				Layer.m_Data = static_cast<const CLayerSoundsValues &>(*pSounds);
			else
			{
				Error = "Unsupported layer type in document capture";
				return std::nullopt;
			}
			vLayers.push_back(ShareRecord(std::move(Layer), PreviousLayers[pLayer->m_Id]));
		}
		const auto *pOldGroup = PreviousGroups[pGroup->m_Id];
		Group.m_Layers = ShareVector(std::move(vLayers), pOldGroup != nullptr ? &pOldGroup->Read().m_Layers : nullptr);
		vGroups.push_back(ShareRecord(std::move(Group), pOldGroup));
	}
	Result.m_Groups = ShareVector(std::move(vGroups), pPrevious != nullptr ? &pPrevious->m_Groups : nullptr);
	if(!Result.Validate(Error))
		return std::nullopt;
	return Result;
}
