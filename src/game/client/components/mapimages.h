/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#define GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#include <game/client/component.h>

class CMapImages : public CComponent
{
	friend class CBackground;

	IGraphics::CTextureHandle m_aTextures[64];
	int m_aTextureUsedByTileOrQuadLayerFlag[64]; // 0: nothing, 1(as flag): tile layer, 2(as flag): quad layer
	int m_Count;

	const char *m_pEntitiesGameType;
public:
	CMapImages();
	CMapImages(int ImageSize);

	IGraphics::CTextureHandle Get(int Index) const { return m_aTextures[Index]; }
	int Num() const { return m_Count; }

	void OnMapLoadImpl(class CLayers *pLayers, class IMap *pMap);
	virtual void OnMapLoad();
	virtual void OnInit();
	void LoadBackground(class CLayers *pLayers, class IMap *pMap);

	// DDRace
	IGraphics::CTextureHandle GetEntities();
	IGraphics::CTextureHandle GetSpeedupArrow();
	
	IGraphics::CTextureHandle GetOverlayBottom();
	IGraphics::CTextureHandle GetOverlayTop();
	IGraphics::CTextureHandle GetOverlayCenter();

	void SetTextureScale(int Size);
	int GetTextureScale();

private:
	bool m_EntitiesIsLoaded;
	bool m_SpeedupArrowIsLoaded;
	IGraphics::CTextureHandle m_EntitiesTextures;
	IGraphics::CTextureHandle m_SpeedupArrowTexture;
	IGraphics::CTextureHandle m_OverlayBottomTexture;
	IGraphics::CTextureHandle m_OverlayTopTexture;
	IGraphics::CTextureHandle m_OverlayCenterTexture;
	int m_TextureScale;
	
	void InitOverlayTextures();
	IGraphics::CTextureHandle UploadEntityLayerText(int TextureSize, int MaxWidth, int YOffset);
	void UpdateEntityLayerText(void* pTexBuffer, int ImageColorChannelCount, int TexWidth, int TexHeight, int TextureSize, int MaxWidth, int YOffset, int NumbersPower, int MaxNumber = -1);
};

#endif
