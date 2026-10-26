/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_MAPITEMS_MAP_H
#define GAME_EDITOR_MAPITEMS_MAP_H

#include <base/types.h>

#include <engine/shared/datafile.h>
#include <engine/shared/jobs.h>

#include <game/editor/editor_history.h>
#include <game/editor/editor_server_settings.h>
#include <game/editor/editor_ui.h>
#include <game/editor/envelope_editor.h>
#include <game/editor/font_typer.h>
#include <game/editor/history/document_history.h>
#include <game/editor/history/save_job.h>
#include <game/editor/map_grid.h>
#include <game/editor/map_view.h>
#include <game/editor/mapitems/document_export.h>
#include <game/editor/mapitems/document_graph.h>
#include <game/editor/mapitems/document_session.h>
#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/envelope_evaluator.h>
#include <game/editor/mapitems/layer.h>
#include <game/editor/proof_mode.h>
#include <game/editor/quad_art.h>
#include <game/editor/quad_knife.h>

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
class CQuadValues;

using FErrorHandler = std::function<void(const char *pErrorMessage)>;

class CEditorMap : public CMapValues
{
public:
	explicit CEditorMap(CEditor *pEditor);

	const CEditor *Editor() const { return m_pEditor; }
	CEditor *Editor() { return m_pEditor; }

	/**
	 * Path and filename including extension within the storage system.
	 */
	char m_aFilename[IO_MAX_PATH_LENGTH];
	/**
	 * Unique name for displaying. Updated by the editor when open maps are changed to ensure it is unique.
	 */
	char m_aDisplayName[IO_MAX_PATH_LENGTH];
	/**
	 * Unique name for autosaving. Updated by the editor when open maps are changed to ensure it is unique.
	 */
	char m_aAutosaveName[IO_MAX_PATH_LENGTH];
	bool m_ValidSaveFilename;
	bool m_CloseOnSave;
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
	std::uint64_t AllocateObjectId();
	/** Copy authoritative values, reuse equal records from pPrevious, then validate. */
	std::optional<CEditorDocumentValues> CaptureDocument(const CEditorDocumentValues *pPrevious, std::string &Error) const;
	/** Remember session choices before a transaction can remove an object. */
	void RememberDocumentSession();
	void PruneDocumentSession(std::span<const CEditorDocumentValues *const> RetainedDocuments);
	/** Prepare and replace runtime bindings; prior-state UI callbacks must have returned. */
	bool RestoreDocumentAtSafePoint(const CEditorDocumentValues &Document, const FErrorHandler &ErrorHandler);
	/** Modification timing/presentation only: does not capture or create a history entry. */
	void OnModify();
	template<typename TUsage>
	void VisitLiveStorage(TUsage &Usage) const;
	void AccountLiveStorage(editor_history::CStorageUsage &Usage) const;
	void AccountRuntimeCaches(editor_history::CStorageUsage &Usage) const;
	void ObserveLiveStorage(editor_history::CStorageObservation &Observation) const;
	void ObserveRuntimeCaches(editor_history::CStorageObservation &Observation) const;
	void ResetModifiedState();
	/** Refresh dirty presentation from history/save markers; does not capture the map. */
	bool RefreshSavedState();
	std::weak_ptr<const CEditorSaveState> Lifetime() const { return m_pSaveState; }
	bool HasLifetime(const std::weak_ptr<const CEditorSaveState> &Lifetime) const { return Lifetime.lock() == m_pSaveState; }
	bool OwnsSave(const CEditorSaveState::CTicket &Ticket) const { return m_pSaveState->Owns(Ticket); }
	bool CompleteSave(const CEditorSaveState::CTicket &Ticket, bool Success);

	// UI elements
	char m_TabSelectButtonId;
	char m_TabCloseButtonId;

	std::vector<std::shared_ptr<CLayerGroup>> m_vpGroups;
	std::vector<std::shared_ptr<CEditorImage>> m_vpImages;
	std::vector<std::shared_ptr<CEnvelope>> m_vpEnvelopes;
	std::vector<std::shared_ptr<CEditorSound>> m_vpSounds;

	std::shared_ptr<CLayerGroup> m_pGameGroup;
	std::shared_ptr<CLayerGame> m_pGameLayer;
	std::shared_ptr<CLayerTele> m_pTeleLayer;
	std::shared_ptr<CLayerSpeedup> m_pSpeedupLayer;
	std::shared_ptr<CLayerFront> m_pFrontLayer;
	std::shared_ptr<CLayerSwitch> m_pSwitchLayer;
	std::shared_ptr<CLayerTune> m_pTuneLayer;

	using CMapInfo = CMapInfoValues;
	CMapInfo m_MapInfoTmp;

	// Undo/Redo
	CEditorDocumentHistory m_DocumentHistory;

	// Selections
	int m_SelectedGroup;
	std::vector<int> m_vSelectedLayers;
	std::vector<int> m_vSelectedQuads;
	int m_SelectedQuadPoints;
	int m_SelectedQuadEnvelope;
	int m_SelectedQuadEnvelopePoint;
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
	bool m_ShowDetail;
	bool m_PreviewZoom;

	// Component states
	CMapView::CState m_MapViewState;
	CMapGrid::CState m_MapGridState;
	CProofMode::CState m_ProofModeState;
	CQuadKnife::CState m_QuadKnifeState;
	CMapEnvelopeEvaluator m_EnvelopeEvaluator;
	CEnvelopeEditor::CState m_EnvelopeEditorState;
	CMapSettingsBackend::CContextWithInput m_MapSettingsCommandContext;
	CFontTyper::CState m_FontTyperState;
	CEditorUiElements m_EditorUiElements;
	CEditorHistoryUiState m_EditorHistoryUiState;

	// Housekeeping
	void CreateDefault();
	void CheckIntegrity();

	// Indices
	int ImageIndex(CDocumentReference Reference) const;
	int SoundIndex(CDocumentReference Reference) const;
	int EnvelopeIndex(CDocumentReference Reference) const;
	CDocumentReference ImageReference(int Index) const;
	CDocumentReference SoundReference(int Index) const;
	CDocumentReference EnvelopeReference(int Index) const;
	void VisitImageReferences(const FDocumentReferenceFunction &ReferenceFunction);
	void VisitAllEnvelopeReferences(const FDocumentReferenceFunction &ReferenceFunction);
	void VisitSoundReferences(const FDocumentReferenceFunction &ReferenceFunction);

	// I/O
	bool Save(const char *pFilename, const FErrorHandler &ErrorHandler);
	bool SaveWithKind(const char *pFilename, editor_history::ESaveKind Kind, const FErrorHandler &ErrorHandler);
	bool PerformPreSaveSanityChecks(const FErrorHandler &ErrorHandler);
	bool Load(const char *pFilename, int StorageType, const FErrorHandler &ErrorHandler);
	bool Append(const char *pFilename, int StorageType, const FErrorHandler &ErrorHandler);
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
	std::vector<CQuadValues *> SelectedQuads();
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
	void DeleteEnvelope(int Index);
	int MoveEnvelope(int IndexFrom, int IndexTo);

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
	CSoundSourceValues *SelectedSoundSource() const;

	void PlaceBorderTiles();

	void AddTileArt(CImageInfo &&Image, const char *pFilename);

	void AddQuadArt(CImageInfo &&Image, const CQuadArtParameters &Parameters);

private:
	friend class CEditorDocumentHistory;
	CDocumentIdentityAllocator m_ObjectIds;
	CDocumentSessionValues m_DocumentSession;
	std::shared_ptr<CEditorSaveState> m_pSaveState = std::make_shared<CEditorSaveState>();
	CEditorDocumentFingerprint m_FingerprintCache;
	std::shared_ptr<const CEditorDocumentValues> m_pLastCapturedDocument;
	std::shared_ptr<const CEditorDocumentValues> CaptureSavedDocument(std::string &Error);
	bool InitializeLoadedSaveState();
	CEditor *m_pEditor;
};

#endif
