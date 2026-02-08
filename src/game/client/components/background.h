#ifndef GAME_CLIENT_COMPONENTS_BACKGROUND_H
#define GAME_CLIENT_COMPONENTS_BACKGROUND_H

#include <engine/map.h>

#include <game/client/components/maplayers.h>

#include <cstdint>
#include <memory>

class CLayers;
class CMapImages;

// Special value to use background of current map
#define CURRENT_MAP "%current%"

class CBackground : public CMapLayers
{
protected:
	IMap *m_pMap;
	bool m_Loaded;
	char m_aMapName[MAX_MAP_LENGTH];

	std::unique_ptr<IMap> m_pBackgroundMap;
	CLayers *m_pBackgroundLayers;
	CMapImages *m_pBackgroundImages;

public:
	CBackground(ERenderType MapType = ERenderType::RENDERTYPE_BACKGROUND_FORCE, bool OnlineOnly = true);
	~CBackground() override;
	int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnMapLoad() override;
	void OnRender() override;

	void LoadBackground();
	const char *MapName() const { return m_aMapName; }
};

#endif
