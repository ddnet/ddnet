/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#define GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#include <game/client/component.h>

class CMapImages : public CComponent
{
	friend class CBackground;

	IGraphics::CTextureHandle m_aTextures[64];
	int m_Count;

	const char *m_pEntitiesGameType;
public:
	CMapImages();
	CMapImages(int ImageSize);

	IGraphics::CTextureHandle Get(int Index) const { return m_aTextures[Index]; }
	int Num() const { return m_Count; }

	virtual void OnMapLoad();
	virtual void OnInit();
	void LoadBackground(class IMap *pMap);

	// DDRace
	IGraphics::CTextureHandle GetEntities();
	
	IGraphics::CTextureHandle GetOverlayBottom();
	IGraphics::CTextureHandle GetOverlayTop();
	IGraphics::CTextureHandle GetOverlayCenter();

	void SetTextureScale(int Size);
	int GetTextureScale();

private:

	bool m_EntitiesIsLoaded;
	IGraphics::CTextureHandle m_EntitiesTextures;
	IGraphics::CTextureHandle m_OverlayBottomTexture;
	IGraphics::CTextureHandle m_OverlayTopTexture;
	IGraphics::CTextureHandle m_OverlayCenterTexture;
	int m_TextureScale;
	
	void InitOverlayTextures();
	IGraphics::CTextureHandle UploadEntityLayerText(int TextureSize, int YOffset);
	void UpdateEntityLayerText(IGraphics::CTextureHandle Texture, int TextureSize, int YOffset, int NumbersPower, int MaxNumber = -1);
};

#endif
