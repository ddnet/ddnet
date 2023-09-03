/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LAYERS_H
#define GAME_LAYERS_H

class IKernel;
class IMap;

struct CMapItemGroup;
struct CMapItemGroupEx;
struct CMapItemLayer;
struct CMapItemLayerTilemap;

class CLayers
{
	int m_GroupsNum;
	int m_GroupsStart;
	int m_GroupsExNum;
	int m_GroupsExStart;
	int m_LayersNum;
	int m_LayersStart;
	CMapItemGroup *m_pGameGroup;
	CMapItemGroupEx *m_pGameGroupEx;
	CMapItemLayerTilemap *m_pGameLayer;
	IMap *m_pMap;

	void InitTilemapSkip();

public:
	CLayers();
	void Init(IKernel *pKernel);
	void InitBackground(IMap *pMap);
	int NumGroups() const { return m_GroupsNum; }
	int NumLayers() const { return m_LayersNum; }
	IMap *Map() const { return m_pMap; }
	CMapItemGroup *GameGroup() const { return m_pGameGroup; }
	CMapItemGroupEx *GameGroupEx() const { return m_pGameGroupEx; }
	CMapItemLayerTilemap *GameLayer() const { return m_pGameLayer; }
	CMapItemGroup *GetGroup(int Index) const;
	CMapItemGroupEx *GetGroupEx(int Index) const;
	CMapItemLayer *GetLayer(int Index) const;

	// DDRace

	CMapItemLayerTilemap *TeleLayer() const { return m_pTeleLayer; }
	CMapItemLayerTilemap *SpeedupLayer() const { return m_pSpeedupLayer; }
	CMapItemLayerTilemap *FrontLayer() const { return m_pFrontLayer; }
	CMapItemLayerTilemap *SwitchLayer() const { return m_pSwitchLayer; }
	CMapItemLayerTilemap *TuneLayer() const { return m_pTuneLayer; }

private:
	CMapItemLayerTilemap *m_pTeleLayer;
	CMapItemLayerTilemap *m_pSpeedupLayer;
	CMapItemLayerTilemap *m_pFrontLayer;
	CMapItemLayerTilemap *m_pSwitchLayer;
	CMapItemLayerTilemap *m_pTuneLayer;
};

#endif
