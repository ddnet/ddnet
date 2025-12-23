/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_MAPITEMS_MAP_H
#define GAME_EDITOR_MAPITEMS_MAP_H

#include <base/types.h>

#include <engine/shared/datafile.h>
#include <engine/shared/jobs.h>

#include <game/editor/editor_history.h>
#include <game/editor/editor_server_settings.h>
#include <game/editor/editor_trackers.h>
#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/layer.h>

#include <functional>
#include <memory>
#include <vector>

class CEditor;
class CEditorImage;
class CEditorSound;
class CLayerFront;
class CLayerGroup;
class CLayerGame;
class CLayerImage;
class CLayerSound;
class CLayerSpeedup;
class CLayerSwitch;
class CLayerTele;
class CLayerTune;
class CQuad;
class IEditorEnvelopeReference;

class CDataFileWriterFinishJob : public IJob
{
	char m_aRealFilename[IO_MAX_PATH_LENGTH];
	char m_aTempFilename[IO_MAX_PATH_LENGTH];
	CDataFileWriter m_Writer;

	void Run() override;

public:
	CDataFileWriterFinishJob(const char *pRealFilename, const char *pTempFilename, CDataFileWriter &&Writer);
	const char *GetRealFilename() const { return m_aRealFilename; }
	const char *GetTempFilename() const { return m_aTempFilename; }
};

using FErrorHandler = std::function<void(const char *pErrorMessage)>;

class CEditorMap
{
public:
	explicit CEditorMap(CEditor *pEditor) :
		m_EditorHistory(this),
		m_ServerSettingsHistory(this),
		m_EnvelopeEditorHistory(this),
		m_QuadTracker(this),
		m_EnvOpTracker(this),
		m_LayerGroupPropTracker(this),
		m_LayerPropTracker(this),
		m_LayerTilesCommonPropTracker(this),
		m_LayerTilesPropTracker(this),
		m_LayerQuadPropTracker(this),
		m_LayerSoundsPropTracker(this),
		m_SoundSourceOperationTracker(this),
		m_SoundSourcePropTracker(this),
		m_SoundSourceRectShapePropTracker(this),
		m_SoundSourceCircleShapePropTracker(this),
		m_pEditor(pEditor)
	{
	}

	const CEditor *Editor() const { return m_pEditor; }
	CEditor *Editor() { return m_pEditor; }

	char m_aFilename[IO_MAX_PATH_LENGTH];
	bool m_ValidSaveFilename;
	/**
	 * Map has unsaved changes for manual save.
	 */
	bool m_Modified;
	/**
	 * Map has unsaved changes for autosave.
	 */
	bool m_ModifiedAuto;
	float m_LastModifiedTime;
	float m_LastSaveTime;
	void OnModify();
	void ResetModifiedState();

	std::vector<std::shared_ptr<CLayerGroup>> m_vpGroups;
	std::vector<std::shared_ptr<CEditorImage>> m_vpImages;
	std::vector<std::shared_ptr<CEnvelope>> m_vpEnvelopes;
	std::vector<std::shared_ptr<CEditorSound>> m_vpSounds;
	std::vector<CEditorMapSetting> m_vSettings;

	std::shared_ptr<CLayerGroup> m_pGameGroup;
	std::shared_ptr<CLayerGame> m_pGameLayer;
	std::shared_ptr<CLayerTele> m_pTeleLayer;
	std::shared_ptr<CLayerSpeedup> m_pSpeedupLayer;
	std::shared_ptr<CLayerFront> m_pFrontLayer;
	std::shared_ptr<CLayerSwitch> m_pSwitchLayer;
	std::shared_ptr<CLayerTune> m_pTuneLayer;

	class CMapInfo
	{
	public:
		char m_aAuthor[32];
		char m_aVersion[16];
		char m_aCredits[128];
		char m_aLicense[32];

		void Reset();
		void Copy(const CMapInfo &Source);
	};
	CMapInfo m_MapInfo;
	CMapInfo m_MapInfoTmp;

	// Undo/Redo
	CEditorHistory m_EditorHistory;
	CEditorHistory m_ServerSettingsHistory;
	CEditorHistory m_EnvelopeEditorHistory;
	CQuadEditTracker m_QuadTracker;
	CEnvelopeEditorOperationTracker m_EnvOpTracker;
	CLayerGroupPropTracker m_LayerGroupPropTracker;
	CLayerPropTracker m_LayerPropTracker;
	CLayerTilesCommonPropTracker m_LayerTilesCommonPropTracker;
	CLayerTilesPropTracker m_LayerTilesPropTracker;
	CLayerQuadsPropTracker m_LayerQuadPropTracker;
	CLayerSoundsPropTracker m_LayerSoundsPropTracker;
	CSoundSourceOperationTracker m_SoundSourceOperationTracker;
	CSoundSourcePropTracker m_SoundSourcePropTracker;
	CSoundSourceRectShapePropTracker m_SoundSourceRectShapePropTracker;
	CSoundSourceCircleShapePropTracker m_SoundSourceCircleShapePropTracker;

	// Selections
	int m_SelectedGroup;
	std::vector<int> m_vSelectedLayers;
	std::vector<int> m_vSelectedQuads;
	int m_SelectedQuadPoints;
	int m_SelectedQuadEnvelope;
	int m_CurrentQuadIndex;
	int m_SelectedEnvelope;
	bool m_UpdateEnvPointInfo;
	std::vector<std::pair<int, int>> m_vSelectedEnvelopePoints;
	std::pair<int, int> m_SelectedTangentInPoint;
	std::pair<int, int> m_SelectedTangentOutPoint;
	int m_SelectedImage;
	int m_SelectedSound;
	int m_SelectedSoundSource;

	int m_ShiftBy;

	// Quad knife
	class CQuadKnife
	{
	public:
		bool m_Active;
		int m_SelectedQuadIndex;
		int m_Count;
		vec2 m_aPoints[4];
	};
	CQuadKnife m_QuadKnife;

	// Housekeeping
	void Clean();
	void CreateDefault();
	void CheckIntegrity();

	// Indices
	void ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction);
	void ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction);
	void ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction);

	// I/O
	bool Save(const char *pFilename, const FErrorHandler &ErrorHandler);
	bool PerformPreSaveSanityChecks(const FErrorHandler &ErrorHandler);
	bool Load(const char *pFilename, int StorageType, const FErrorHandler &ErrorHandler);
	void PerformSanityChecks(const FErrorHandler &ErrorHandler);
	bool PerformAutosave(const FErrorHandler &ErrorHandler);

	// Groups
	std::shared_ptr<CLayerGroup> SelectedGroup() const;
	std::shared_ptr<CLayerGroup> NewGroup();
	int MoveGroup(int IndexFrom, int IndexTo);
	void DeleteGroup(int Index);
	void MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup);

	// Layers
	std::shared_ptr<CLayer> SelectedLayer(int Index) const;
	std::shared_ptr<CLayer> SelectedLayerType(int Index, int Type) const;
	void SelectLayer(int LayerIndex, int GroupIndex = -1);
	void AddSelectedLayer(int LayerIndex);
	void SelectNextLayer();
	void SelectPreviousLayer();
	void SelectGameLayer();
	void MakeGameLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer);

	// Quads
	std::vector<CQuad *> SelectedQuads();
	bool IsQuadSelected(int Index) const;
	int FindSelectedQuadIndex(int Index) const;
	void SelectQuad(int Index);
	void ToggleSelectQuad(int Index);
	void DeselectQuads();
	bool IsQuadCornerSelected(int Index) const;
	bool IsQuadPointSelected(int QuadIndex, int Index) const;
	void SelectQuadPoint(int QuadIndex, int Index);
	void ToggleSelectQuadPoint(int QuadIndex, int Index);
	void DeselectQuadPoints();
	void DeleteSelectedQuads();

	// Envelopes
	std::shared_ptr<CEnvelope> NewEnvelope(CEnvelope::EType Type);
	void InsertEnvelope(int Index, std::shared_ptr<CEnvelope> &pEnvelope);
	void UpdateEnvelopeReferences(int Index, std::shared_ptr<CEnvelope> &pEnvelope, std::vector<std::shared_ptr<IEditorEnvelopeReference>> &vpEditorObjectReferences);
	std::vector<std::shared_ptr<IEditorEnvelopeReference>> DeleteEnvelope(int Index);
	int MoveEnvelope(int IndexFrom, int IndexTo);
	template<typename F>
	std::vector<std::shared_ptr<IEditorEnvelopeReference>> VisitEnvelopeReferences(F &&Visitor);
	bool IsEnvelopeUsed(int EnvelopeIndex) const;
	void RemoveUnusedEnvelopes();

	// Envelope points
	int FindEnvPointIndex(int Index, int Channel) const;
	void SelectEnvPoint(int Index);
	void SelectEnvPoint(int Index, int Channel);
	void ToggleEnvPoint(int Index, int Channel);
	bool IsEnvPointSelected(int Index, int Channel) const;
	bool IsEnvPointSelected(int Index) const;
	void DeselectEnvPoints();
	bool IsTangentSelected() const;
	bool IsTangentOutPointSelected(int Index, int Channel) const;
	bool IsTangentOutSelected() const;
	void SelectTangentOutPoint(int Index, int Channel);
	bool IsTangentInPointSelected(int Index, int Channel) const;
	bool IsTangentInSelected() const;
	void SelectTangentInPoint(int Index, int Channel);
	std::pair<CFixedTime, int> SelectedEnvelopeTimeAndValue() const;

	// Images
	std::shared_ptr<CEditorImage> SelectedImage() const;
	void SelectImage(const std::shared_ptr<CEditorImage> &pImage);
	void SelectNextImage();
	void SelectPreviousImage();
	bool IsImageUsed(int ImageIndex) const;
	std::vector<int> SortImages();

	// Sounds
	std::shared_ptr<CEditorSound> SelectedSound() const;
	void SelectSound(const std::shared_ptr<CEditorSound> &pSound);
	void SelectNextSound();
	void SelectPreviousSound();
	bool IsSoundUsed(int SoundIndex) const;
	CSoundSource *SelectedSoundSource() const;

	void PlaceBorderTiles();

private:
	CEditor *m_pEditor;
};

#endif
