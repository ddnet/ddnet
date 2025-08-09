/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_MAPITEMS_MAP_H
#define GAME_EDITOR_MAPITEMS_MAP_H

#include <base/types.h>

#include <engine/shared/datafile.h>
#include <engine/shared/jobs.h>

#include <game/editor/editor_server_settings.h>
#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/layer.h>
#include <game/editor/references.h>

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

class CDataFileWriterFinishJob : public IJob
{
	char m_aRealFileName[IO_MAX_PATH_LENGTH];
	char m_aTempFileName[IO_MAX_PATH_LENGTH];
	CDataFileWriter m_Writer;

	void Run() override;

public:
	CDataFileWriterFinishJob(const char *pRealFileName, const char *pTempFileName, CDataFileWriter &&Writer);
	const char *GetRealFileName() const { return m_aRealFileName; }
	const char *GetTempFileName() const { return m_aTempFileName; }
};

class CEditorMap
{
public:
	explicit CEditorMap(CEditor *pEditor) :
		m_pEditor(pEditor)
	{
		Clean();
	}

	CEditor *Editor() const { return m_pEditor; }

	bool m_Modified; // unsaved changes in manual save
	bool m_ModifiedAuto; // unsaved changes in autosave
	float m_LastModifiedTime;
	float m_LastSaveTime;
	float m_LastAutosaveUpdateTime;
	void OnModify();

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

	std::shared_ptr<CEnvelope> NewEnvelope(CEnvelope::EType Type);
	void InsertEnvelope(int Index, std::shared_ptr<CEnvelope> &pEnvelope);
	void UpdateEnvelopeReferences(int Index, std::shared_ptr<CEnvelope> &pEnvelope, std::vector<std::shared_ptr<IEditorEnvelopeReference>> &vpEditorObjectReferences);
	std::vector<std::shared_ptr<IEditorEnvelopeReference>> DeleteEnvelope(int Index);
	int MoveEnvelope(int IndexFrom, int IndexTo);
	template<typename F>
	std::vector<std::shared_ptr<IEditorEnvelopeReference>> VisitEnvelopeReferences(F &&Visitor);

	std::shared_ptr<CLayerGroup> NewGroup();
	int MoveGroup(int IndexFrom, int IndexTo);
	void DeleteGroup(int Index);
	void ModifyImageIndex(const FIndexModifyFunction &IndexModifyFunction);
	void ModifyEnvelopeIndex(const FIndexModifyFunction &IndexModifyFunction);
	void ModifySoundIndex(const FIndexModifyFunction &IndexModifyFunction);

	void Clean();
	void CreateDefault(IGraphics::CTextureHandle EntitiesTexture);

	// io
	bool Save(const char *pFilename, const std::function<void(const char *pErrorMessage)> &ErrorHandler);
	bool PerformPreSaveSanityChecks(const std::function<void(const char *pErrorMessage)> &ErrorHandler);
	bool Load(const char *pFilename, int StorageType, const std::function<void(const char *pErrorMessage)> &ErrorHandler);
	void PerformSanityChecks(const std::function<void(const char *pErrorMessage)> &ErrorHandler);

	void MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup);
	void MakeGameLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer);

private:
	CEditor *m_pEditor;
};

#endif
