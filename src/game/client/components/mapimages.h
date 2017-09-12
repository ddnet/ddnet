/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#define GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#include <game/client/component.h>

class CMapImages : public CComponent
{
	friend class CBackground;

	int m_aTextures[64];
	int m_Count;

	char m_aEntitiesGameType[16];
public:
	CMapImages();

	int Get(int Index) const { return m_aTextures[Index]; }
	int Num() const { return m_Count; }

	virtual void OnMapLoad();
	virtual void OnInit();
	void LoadBackground(class IMap *pMap);

	// DDRace

	int GetEntities();
	
	int GetOverlayBottom();
	int GetOverlayTop();
	int GetOverlayCenter();

private:

	int m_EntitiesTextures;
	int m_OverlayBottomTexture;
	int m_OverlayTopTexture;
	int m_OverlayCenterTexture;
};

#endif
