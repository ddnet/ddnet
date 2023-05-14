/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LAYERS_H
#define GAME_LAYERS_H

class IMap;

struct CMapItemGroup;
struct CMapItemLayer;
struct CMapItemLayerTilemap;

class CLayers
{
public:
	CLayers();
	void Init(IMap *pMap, bool GameOnly);
	void Unload();

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
	int m_GroupsNum;
	int m_GroupsStart;
	int m_LayersNum;
	int m_LayersStart;

	CMapItemGroup *m_pGameGroup;
	CMapItemLayerTilemap *m_pGameLayer;
	IMap *m_pMap;

	CMapItemLayerTilemap *m_pTeleLayer;
	CMapItemLayerTilemap *m_pSpeedupLayer;
	CMapItemLayerTilemap *m_pFrontLayer;
	CMapItemLayerTilemap *m_pSwitchLayer;
	CMapItemLayerTilemap *m_pTuneLayer;

	void InitTilemapSkip();
};

#endif
