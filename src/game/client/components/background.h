#ifndef GAME_CLIENT_COMPONENTS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_BACKGROUND_H
#include <game/client/component.h>
#include <game/client/components/maplayers.h>

// Special value to use background of current map
#define CURRENT "%current%"

class CBackground : public CMapLayers
{
	IEngineMap *m_pMap;
	bool m_Loaded;
	char m_aMapName[128];

	//to avoid spam when in menu
	int64 m_LastLoad;

	//to avoid memory leak when switching to %current%
	IEngineMap *m_pBackgroundMap;
	CLayers *m_pBackgroundLayers;
	CMapImages *m_pBackgroundImages;

public:
	CBackground();
	~CBackground();

	virtual void OnInit();
	virtual void OnMapLoad();
	virtual void OnRender();

	void LoadBackground();
};

#endif
