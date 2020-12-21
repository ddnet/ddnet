#ifndef GAME_CLIENT_COMPONENTS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_BACKGROUND_H
#include <engine/shared/map.h>
#include <game/client/component.h>
#include <game/client/components/maplayers.h>

// Special value to use background of current map
#define CURRENT_MAP "%current%"

class CBackgroundEngineMap : public CMap
{
	MACRO_INTERFACE("background_enginemap", 0)
};

class CBackground : public CMapLayers
{
protected:
	IEngineMap *m_pMap;
	bool m_Loaded;
	char m_aMapName[MAX_MAP_LENGTH];

	//to avoid spam when in menu
	int64 m_LastLoad;

	//to avoid memory leak when switching to %current%
	CBackgroundEngineMap *m_pBackgroundMap;
	CLayers *m_pBackgroundLayers;
	CMapImages *m_pBackgroundImages;

	virtual CBackgroundEngineMap *CreateBGMap();

public:
	CBackground(int MapType = CMapLayers::TYPE_BACKGROUND_FORCE, bool OnlineOnly = true);
	virtual ~CBackground();

	virtual void OnInit();
	virtual void OnMapLoad();
	virtual void OnRender();

	void LoadBackground();
};

#endif
