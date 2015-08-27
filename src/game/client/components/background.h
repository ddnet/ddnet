#ifndef GAME_CLIENT_COMPONENTS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_BACKGROUND_H
#include <game/client/component.h>

class CBackground : public CComponent
{
	class CMapLayers *m_pLayers;
	class CMapImages *m_pImages;
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
