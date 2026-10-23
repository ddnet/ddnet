#ifndef GAME_EDITOR_MAPITEMS_DOCUMENT_GRAPH_H
#define GAME_EDITOR_MAPITEMS_DOCUMENT_GRAPH_H

#include "document_values.h"

#include <game/editor/history/shared_value.h>

#include <string>
#include <unordered_set>
#include <variant>

/** The variant preserves both base and specialized semantic planes. */
class CTileLayerDocumentValues : public CLayerTilesValues
{
public:
	std::variant<std::monostate, CLayerTeleValues, CLayerSpeedupValues, CLayerSwitchValues, CLayerTuneValues> m_Auxiliary;
	bool operator==(const CTileLayerDocumentValues &) const = default;
};

/** New layer kinds need graph/capture/restore integration. */
class CLayerDocumentValues : public CLayerValues
{
public:
	std::variant<CTileLayerDocumentValues, CLayerQuadsValues, CLayerSoundsValues> m_Data;
	bool operator==(const CLayerDocumentValues &) const = default;
};

class CGroupDocumentValues : public CLayerGroupValues
{
public:
	editor_history::CSharedVector<editor_history::CSharedValue<CLayerDocumentValues>> m_Layers;
	bool operator==(const CGroupDocumentValues &) const = default;
};

/**
 * Complete value-only document revision. Ordering and identities
 * are independent. Canonical references retain identities independently of collection order.
 * Capture this at explicit edit boundaries, never from rendering or OnModify.
 */
class CEditorDocumentValues : public CMapValues
{
public:
	editor_history::CSharedVector<editor_history::CSharedValue<CGroupDocumentValues>> m_Groups;
	editor_history::CSharedVector<CEditorImageValues> m_Images;
	editor_history::CSharedVector<CEditorSoundValues> m_Sounds;
	editor_history::CSharedVector<editor_history::CSharedValue<CEnvelopeValues>> m_Envelopes;
	bool operator==(const CEditorDocumentValues &) const = default;

	int EnvelopeIndex(CDocumentReference Reference) const
	{
		for(std::size_t Index = 0; Index < m_Envelopes.Read().size(); ++Index)
			if(m_Envelopes.Read()[Index].Read().m_Id == Reference.m_Id)
				return static_cast<int>(Index);
		return -1;
	}

	CDocumentReference EnvelopeReference(int Index) const
	{
		return {Index >= 0 && static_cast<std::size_t>(Index) < m_Envelopes.Read().size() ? m_Envelopes.Read()[Index].Read().m_Id : 0};
	}

	/** Validation never repairs or mutates a recorded result. */
	bool Validate(std::string &Error) const
	{
		Error.clear();
		std::unordered_set<std::uint64_t> Ids;
		const auto Identity = [&](std::uint64_t Id) {
			return Id != 0 && Id != std::numeric_limits<std::uint64_t>::max() && Ids.insert(Id).second;
		};
		const auto Fail = [&](const char *pMessage) {
			Error = pMessage;
			return false;
		};
		const auto Reference = [](CDocumentReference Target, const auto &Values) {
			return !Target.IsSet() || std::any_of(Values.begin(), Values.end(), [&](const auto &Value) {
				if constexpr(requires { Value.Read(); })
					return Value.Read().m_Id == Target.m_Id;
				else
					return Value.m_Id == Target.m_Id;
			});
		};
		for(const auto &Image : m_Images.Read())
		{
			if(!Identity(Image.m_Id))
				return Fail("Duplicate or missing image identity");
			if(Image.m_Content)
			{
				if(Image.m_Format != CImageInfo::FORMAT_RGB && Image.m_Format != CImageInfo::FORMAT_RGBA && Image.m_Format != CImageInfo::FORMAT_R && Image.m_Format != CImageInfo::FORMAT_RA)
					return Fail("Invalid image format");
				const auto PixelSize = CImageInfo::PixelSize(Image.m_Format);
				if(Image.m_Width == 0 || Image.m_Height == 0 ||
					Image.m_Width > std::numeric_limits<std::size_t>::max() / Image.m_Height / PixelSize ||
					Image.DataSize() != Image.m_Content->Bytes().size())
					return Fail("Invalid image content dimensions");
			}
		}
		for(const auto &Sound : m_Sounds.Read())
			if(!Identity(Sound.m_Id))
				return Fail("Duplicate or missing sound identity");
		for(const auto &EnvelopeRecord : m_Envelopes.Read())
		{
			const auto &Envelope = EnvelopeRecord.Read();
			if(!Identity(Envelope.m_Id))
				return Fail("Duplicate or missing envelope identity");
			if(Envelope.m_Type != CEnvelopeValues::EType::POSITION && Envelope.m_Type != CEnvelopeValues::EType::COLOR && Envelope.m_Type != CEnvelopeValues::EType::SOUND)
				return Fail("Invalid envelope type");
			for(std::size_t Index = 0; Index < Envelope.m_vPoints.size(); ++Index)
			{
				const auto &Point = Envelope.m_vPoints[Index];
				if(!Identity(Point.m_Id))
					return Fail("Duplicate or missing envelope point identity");
				if(Index != 0 && Point.m_Time < Envelope.m_vPoints[Index - 1].m_Time)
					return Fail("Envelope point order does not match its times");
			}
		}
		const CTileLayerDocumentValues *pGame = nullptr;
		std::vector<const CTileLayerDocumentValues *> vpSpecial;
		std::array<bool, 6> aRoles{};
		bool GameGroup = false;
		for(const auto &GroupRecord : m_Groups.Read())
		{
			const auto &Group = GroupRecord.Read();
			if(!Identity(Group.m_Id))
				return Fail("Duplicate or missing group identity");
			if(Group.m_GameGroup && std::exchange(GameGroup, true))
				return Fail("Multiple game groups");
			for(const auto &LayerRecord : Group.m_Layers.Read())
			{
				const auto &Layer = LayerRecord.Read();
				if(!Identity(Layer.m_Id))
					return Fail("Duplicate or missing layer identity");
				if(const auto *pTiles = std::get_if<CTileLayerDocumentValues>(&Layer.m_Data))
				{
					if(Layer.m_Type != LAYERTYPE_TILES || pTiles->Width() <= 0 || pTiles->Height() <= 0)
						return Fail("Invalid tile layer type or dimensions");
					if(!Reference(pTiles->m_Image, m_Images.Read()) || !Reference(pTiles->m_ColorEnv, m_Envelopes.Read()))
						return Fail("Unresolved tile layer resource reference");
					const std::array<bool, 6> aLayerRoles{pTiles->m_HasGame, pTiles->m_HasFront, pTiles->m_HasTele, pTiles->m_HasSpeedup, pTiles->m_HasSwitch, pTiles->m_HasTune};
					int RoleCount = 0;
					for(std::size_t Index = 0; Index < aLayerRoles.size(); ++Index)
					{
						if(!aLayerRoles[Index])
							continue;
						if(!Group.m_GameGroup || std::exchange(aRoles[Index], true))
							return Fail("Duplicate special role or special layer outside game group");
						++RoleCount;
					}
					if(RoleCount > 1)
						return Fail("Conflicting tile layer roles");
					if(pTiles->m_HasGame)
						pGame = pTiles;
					else if(RoleCount != 0)
						vpSpecial.push_back(pTiles);
					const auto MatchingPlane = [&](const auto &Plane) {
						return Plane.Width() == pTiles->Width() && Plane.Height() == pTiles->Height();
					};
					bool ValidAuxiliary = false;
					std::visit([&](const auto &Auxiliary) {
						using T = std::decay_t<decltype(Auxiliary)>;
						if constexpr(std::is_same_v<T, std::monostate>)
							ValidAuxiliary = !pTiles->m_HasTele && !pTiles->m_HasSpeedup && !pTiles->m_HasSwitch && !pTiles->m_HasTune;
						else if constexpr(std::is_same_v<T, CLayerTeleValues>)
							ValidAuxiliary = pTiles->m_HasTele && MatchingPlane(Auxiliary.m_TeleTiles);
						else if constexpr(std::is_same_v<T, CLayerSpeedupValues>)
							ValidAuxiliary = pTiles->m_HasSpeedup && MatchingPlane(Auxiliary.m_SpeedupTiles);
						else if constexpr(std::is_same_v<T, CLayerSwitchValues>)
							ValidAuxiliary = pTiles->m_HasSwitch && MatchingPlane(Auxiliary.m_SwitchTiles);
						else if constexpr(std::is_same_v<T, CLayerTuneValues>)
							ValidAuxiliary = pTiles->m_HasTune && MatchingPlane(Auxiliary.m_TuneTiles);
					},
						pTiles->m_Auxiliary);
					if(!ValidAuxiliary)
						return Fail("Special tile payload does not match its role or dimensions");
				}
				else if(const auto *pQuads = std::get_if<CLayerQuadsValues>(&Layer.m_Data))
				{
					if(Layer.m_Type != LAYERTYPE_QUADS || !Reference(pQuads->m_Image, m_Images.Read()))
						return Fail("Invalid quad layer type or image reference");
					for(const auto &Quad : pQuads->m_vQuads)
						if(!Identity(Quad.m_Id) || !Reference(Quad.m_PosEnv, m_Envelopes.Read()) || !Reference(Quad.m_ColorEnv, m_Envelopes.Read()))
							return Fail("Invalid quad identity or envelope reference");
				}
				else if(const auto *pSounds = std::get_if<CLayerSoundsValues>(&Layer.m_Data))
				{
					if(Layer.m_Type != LAYERTYPE_SOUNDS || !Reference(pSounds->m_Sound, m_Sounds.Read()))
						return Fail("Invalid sound layer type or sound reference");
					for(const auto &Source : pSounds->m_vSources)
						if(!Identity(Source.m_Id) || !Reference(Source.m_PosEnv, m_Envelopes.Read()) || !Reference(Source.m_SoundEnv, m_Envelopes.Read()) ||
							(Source.m_Shape.m_Type != CSoundShape::SHAPE_CIRCLE && Source.m_Shape.m_Type != CSoundShape::SHAPE_RECTANGLE))
							return Fail("Invalid sound source identity, envelope reference or shape");
				}
			}
		}
		if(!GameGroup || pGame == nullptr)
			return Fail("Missing game group or layer");
		for(const auto *pSpecial : vpSpecial)
			if(pSpecial->Width() != pGame->Width() || pSpecial->Height() != pGame->Height())
				return Fail("Special layer dimensions differ from the game layer");
		return true;
	}

	/**
	 * Capture allocation identities and immutable owners for worker accounting.
	 * New root fields must be included here as well as in Account; copying just
	 * the shared graph below intentionally avoids copying raw mutable vectors.
	 */
	void ObserveStorage(editor_history::CStorageObservation &Observation) const
	{
		// Settings are a value vector. Observe the original allocation identity and
		// capacity without copying it into a measurement-only document allocation.
		Observation.Add(m_vSettings.data(), m_vSettings.capacity() * sizeof(CEditorMapSetting));
		CEditorDocumentValues Shared;
		Shared.m_Images = m_Images;
		Shared.m_Sounds = m_Sounds;
		Shared.m_Envelopes = m_Envelopes;
		Shared.m_Groups = m_Groups;
		Observation.ObserveOwned([Document = std::move(Shared)](editor_history::CStorageUsage &Usage) { Document.Account(Usage); });
	}

	/**
	 * Visit owned payload/capacity, descending only on first-add/last-remove of a
	 * shared owner. Keep heap-field changes aligned with ObserveStorage and the
	 * runtime CEditorMap::VisitLiveStorage traversal; these are not process bytes.
	 */
	void Account(editor_history::CStorageUsage &Usage) const
	{
		const auto AccountVector = [&](const auto &vValues) {
			Usage.Add(vValues.data(), vValues.capacity() * sizeof(typename std::decay_t<decltype(vValues)>::value_type));
		};
		AccountVector(m_vSettings);
		if(m_Images.Account(Usage))
			for(const auto &Image : m_Images.Read())
				if(Image.m_Content)
					Image.m_Content->Account(Usage);
		if(m_Sounds.Account(Usage))
			for(const auto &Sound : m_Sounds.Read())
				if(Sound.m_Content)
					Sound.m_Content->Account(Usage);
		if(m_Envelopes.Account(Usage))
			for(const auto &Envelope : m_Envelopes.Read())
				if(Envelope.Account(Usage))
					AccountVector(Envelope.Read().m_vPoints);
		if(m_Groups.Account(Usage))
			for(const auto &Group : m_Groups.Read())
				if(Group.Account(Usage) && Group.Read().m_Layers.Account(Usage))
					for(const auto &Layer : Group.Read().m_Layers.Read())
						if(Layer.Account(Usage))
							std::visit([&](const auto &Data) {
								using T = std::decay_t<decltype(Data)>;
								if constexpr(std::is_same_v<T, CTileLayerDocumentValues>)
								{
									Data.m_Tiles.Account(Usage);
									std::visit([&](const auto &Auxiliary) {
										using A = std::decay_t<decltype(Auxiliary)>;
										if constexpr(std::is_same_v<A, CLayerTeleValues>)
											Auxiliary.m_TeleTiles.Account(Usage);
										else if constexpr(std::is_same_v<A, CLayerSpeedupValues>)
											Auxiliary.m_SpeedupTiles.Account(Usage);
										else if constexpr(std::is_same_v<A, CLayerSwitchValues>)
											Auxiliary.m_SwitchTiles.Account(Usage);
										else if constexpr(std::is_same_v<A, CLayerTuneValues>)
											Auxiliary.m_TuneTiles.Account(Usage);
									},
										Data.m_Auxiliary);
								}
								else if constexpr(std::is_same_v<T, CLayerQuadsValues>)
									AccountVector(Data.m_vQuads);
								else if constexpr(std::is_same_v<T, CLayerSoundsValues>)
									AccountVector(Data.m_vSources);
							},
								Layer.Read().m_Data);
	}
};

#endif
