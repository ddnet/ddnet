#ifndef GAME_CLIENT_COMPONENTS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_BACKGROUND_H

#include <engine/shared/map.h>

#include <game/client/components/maplayers.h>

#include <stdint.h>

class CLayers;
class CMapImages;

// Special value to use background of current map
#define CURRENT_MAP "%current%"

class CBackgroundEngineMap : public CMap
{
	MACRO_INTERFACE("background_enginemap", 0)
};

class CBackground : public CMapLayers
{
protected:
	IEngineMap *m_pMap = nullptr;
	bool m_Loaded = false;
	char m_aMapName[MAX_MAP_LENGTH] = {0};

	//to avoid spam when in menu
	int64_t m_LastLoad = 0;

	//to avoid memory leak when switching to %current%
	CBackgroundEngineMap *m_pBackgroundMap = nullptr;
	CLayers *m_pBackgroundLayers = nullptr;
	CMapImages *m_pBackgroundImages = nullptr;

	virtual CBackgroundEngineMap *CreateBGMap();

public:
	CBackground(int MapType = CMapLayers::TYPE_BACKGROUND_FORCE, bool OnlineOnly = true);
	virtual ~CBackground();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnInit() override;
	virtual void OnMapLoad() override;
	virtual void OnRender() override;

	void LoadBackground();
};

#endif
