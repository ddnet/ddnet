#ifndef GAME_CLIENT_COMPONENTS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_BACKGROUND_H

#include <engine/shared/map.h>

#include <game/client/components/maplayers.h>

#include <cstdint>

class CLayers;
class CMapImages;

// Special value to use background of current map
#define CURRENT_MAP "%current%"

class CBackgroundEngineMap : public CMap
{
	MACRO_INTERFACE("background_enginemap")
};

class CBackground : public CMapLayers
{
protected:
	IEngineMap *m_pMap;
	bool m_Loaded;
	char m_aMapName[MAX_MAP_LENGTH];

	//to avoid memory leak when switching to %current%
	CBackgroundEngineMap *m_pBackgroundMap;
	CLayers *m_pBackgroundLayers;
	CMapImages *m_pBackgroundImages;

	virtual CBackgroundEngineMap *CreateBGMap();

	const char *LoadingTitle() const override;

public:
	CBackground(int MapType = CMapLayers::TYPE_BACKGROUND_FORCE, bool OnlineOnly = true);
	virtual ~CBackground();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnInit() override;
	virtual void OnMapLoad() override;
	virtual void OnRender() override;

	void LoadBackground();
	const char *MapName() const { return m_aMapName; }
};

#endif
