#ifndef GAME_EDITOR_MAPITEMS_MAP_H
#define GAME_EDITOR_MAPITEMS_MAP_H

#include <game/editor/component.h>

class CLayerGroup;
class CLayer;
class CEnvelope;
class CEditorImage;
class CEditorSound;

using FIndexModifyFunction = std::function<void(int *pIndex)>;

class CEditorMap : public CEditorComponent
{
	void MakeGameGroup(std::shared_ptr<CLayerGroup> pGroup);
	void MakeGameLayer(const std::shared_ptr<CLayer> &pLayer);

public:
	CEditorMap();
	~CEditorMap();

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

	struct CSetting
	{
		char m_aCommand[256];

		CSetting(const char *pCommand);
	};
	std::vector<CSetting> m_vSettings;

	std::shared_ptr<class CLayerGame> m_pGameLayer;
	std::shared_ptr<CLayerGroup> m_pGameGroup;

	std::shared_ptr<CEnvelope> NewEnvelope(int Channels);

	void DeleteEnvelope(int Index);
	void SwapEnvelopes(int Index0, int Index1);
	template<typename F>
	void VisitEnvelopeReferences(F &&Visitor);

	std::shared_ptr<CLayerGroup> NewGroup();
	int SwapGroups(int Index0, int Index1);
	void DeleteGroup(int Index);

	void ModifyImageIndex(const FIndexModifyFunction &pfnFunc);
	void ModifyEnvelopeIndex(const FIndexModifyFunction &pfnFunc);
	void ModifySoundIndex(const FIndexModifyFunction &pfnFunc);

	void Clean();
	void CreateDefault(IGraphics::CTextureHandle EntitiesTexture);

	// io
	bool Save(const char *pFilename);
	bool Load(const char *pFilename, int StorageType, const std::function<void(const char *pErrorMessage)> &ErrorHandler);
	void PerformSanityChecks(const std::function<void(const char *pErrorMessage)> &ErrorHandler);

	// DDRace

	std::shared_ptr<class CLayerTele> m_pTeleLayer;
	std::shared_ptr<class CLayerSpeedup> m_pSpeedupLayer;
	std::shared_ptr<class CLayerFront> m_pFrontLayer;
	std::shared_ptr<class CLayerSwitch> m_pSwitchLayer;
	std::shared_ptr<class CLayerTune> m_pTuneLayer;
	void MakeTeleLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSpeedupLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeFrontLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeSwitchLayer(const std::shared_ptr<CLayer> &pLayer);
	void MakeTuneLayer(const std::shared_ptr<CLayer> &pLayer);
};

#endif
