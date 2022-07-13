/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LAYERS_H
#define GAME_LAYERS_H

class IKernel;
class IMap;

struct CMapItemGroup;
struct CMapItemLayer;
struct CMapItemLayerTilemap;

class CLayers
{
	int m_GroupsNum = 0;
	int m_GroupsStart = 0;
	int m_LayersNum = 0;
	int m_LayersStart = 0;
	CMapItemGroup *m_pGameGroup = nullptr;
	CMapItemLayerTilemap *m_pGameLayer = nullptr;
	IMap *m_pMap = nullptr;

	void InitTilemapSkip();

public:
	CLayers();
	void Init(IKernel *pKernel);
	void InitBackground(IMap *pMap);
	int NumGroups() const { return m_GroupsNum; }
	int NumLayers() const { return m_LayersNum; }
	IMap *Map() const { return m_pMap; }
	CMapItemGroup *GameGroup() const { return m_pGameGroup; }
	CMapItemLayerTilemap *GameLayer() const { return m_pGameLayer; }
	CMapItemGroup *GetGroup(int Index) const;
	CMapItemLayer *GetLayer(int Index) const;

	// DDRace

	CMapItemLayerTilemap *TeleLayer() const { return m_pTeleLayer; }
	CMapItemLayerTilemap *SpeedupLayer() const { return m_pSpeedupLayer; }
	CMapItemLayerTilemap *FrontLayer() const { return m_pFrontLayer; }
	CMapItemLayerTilemap *SwitchLayer() const { return m_pSwitchLayer; }
	CMapItemLayerTilemap *TuneLayer() const { return m_pTuneLayer; }

private:
	CMapItemLayerTilemap *m_pTeleLayer = nullptr;
	CMapItemLayerTilemap *m_pSpeedupLayer = nullptr;
	CMapItemLayerTilemap *m_pFrontLayer = nullptr;
	CMapItemLayerTilemap *m_pSwitchLayer = nullptr;
	CMapItemLayerTilemap *m_pTuneLayer = nullptr;
};

#endif
