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
	void LoadBackground(class IMap *pMap);

	// DDRace

	int GetEntities();

private:

	int m_EntitiesTextures;
};

#endif
