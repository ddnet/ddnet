/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LAYERS_H
#define GAME_LAYERS_H
#include<vector>

class IMap;

class CMapItemGroup;
class CMapItemLayer;
class CMapItemLayerTilemap;

// <FoxNet
class CMapItemLayerQuads;
constexpr char ValidQuadNames[5][30] = {
	"QFr",
	"QUnFr",
	"QDeath",
	"QStopa",
	"QCfrm"
};

enum QuadTypes
{
	// Follows order of ValidQuadNames
	QUADTYPE_NONE = -1,
	QUADTYPE_FREEZE,
	QUADTYPE_UNFREEZE,
	QUADTYPE_DEATH,
	QUADTYPE_STOPA,
	QUADTYPE_CFRM,
	NUM_QUADTYPES
};
// FoxNet>

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
	// <FoxNet
	const std::vector<CMapItemLayerQuads *> &QuadLayers() const { return m_vQuadLayers; }
	// FoxNet>

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
	// <FoxNet
	std::vector<CMapItemLayerQuads *> m_vQuadLayers;
	// FoxNet>

	void InitTilemapSkip();
};

#endif
