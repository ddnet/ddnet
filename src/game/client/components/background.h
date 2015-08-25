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
	
public:
	CBackground();
	~CBackground();
	
	virtual void OnInit();
	virtual void OnMapLoad();
	virtual void OnRender();
	
};

#endif