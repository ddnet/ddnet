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

	const char *m_pEntitiesGameType;
public:
	CMapImages();
	CMapImages(int ImageSize);

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

	void SetTextureScale(int Size);
	int GetTextureScale();

private:

	int m_EntitiesTextures;
	int m_OverlayBottomTexture;
	int m_OverlayTopTexture;
	int m_OverlayCenterTexture;
	int m_TextureScale;
	
	void InitOverlayTextures();
	int UploadEntityLayerText(int TextureSize, int YOffset);
};

#endif
