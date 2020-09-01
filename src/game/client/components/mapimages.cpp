/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/storage.h>
#include <engine/serverbrowser.h>
#include <game/client/component.h>
#include <engine/textrender.h>
#include <game/mapitems.h>

#include "mapimages.h"

CMapImages::CMapImages() : CMapImages(100)
{
}

CMapImages::CMapImages(int TextureSize) 
{
	m_Count = 0;
	m_TextureScale = TextureSize;
	m_EntitiesIsLoaded = false;
	m_SpeedupArrowIsLoaded = false;

	mem_zero(m_aTextureUsedByTileOrQuadLayerFlag, sizeof(m_aTextureUsedByTileOrQuadLayerFlag));
}

void CMapImages::OnInit()
{
	InitOverlayTextures();
}

void CMapImages::OnMapLoadImpl(class CLayers *pLayers, IMap *pMap)
{
	// unload all textures
	for(int i = 0; i < m_Count; i++)
	{
		Graphics()->UnloadTexture(m_aTextures[i]);
		m_aTextures[i] = IGraphics::CTextureHandle();
		m_aTextureUsedByTileOrQuadLayerFlag[i] = 0;
	}
	m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);

	for(int g = 0; g < pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = pLayers->GetGroup(g);
		if(!pGroup)
		{
			continue;
		}

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer+l);
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTLayer = (CMapItemLayerTilemap *)pLayer;
				if(pTLayer->m_Image != -1 && pTLayer->m_Image < (int)(sizeof(m_aTextures) / sizeof(m_aTextures[0])))
				{
					m_aTextureUsedByTileOrQuadLayerFlag[(size_t)pTLayer->m_Image] |= 1;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
				if(pQLayer->m_Image != -1 && pQLayer->m_Image < (int)(sizeof(m_aTextures) / sizeof(m_aTextures[0])))
				{
					m_aTextureUsedByTileOrQuadLayerFlag[(size_t)pQLayer->m_Image] |= 2;
				}
			}
		}
	}

	int TextureLoadFlag = Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;

	// load new textures
	for(int i = 0; i < m_Count; i++)
	{
		int LoadFlag = (((m_aTextureUsedByTileOrQuadLayerFlag[i] & 1) != 0) ? TextureLoadFlag : 0) | (((m_aTextureUsedByTileOrQuadLayerFlag[i] & 2) != 0) ? 0 : (Graphics()->IsTileBufferingEnabled() ? IGraphics::TEXLOAD_NO_2D_TEXTURE : 0));
		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start+i, 0, 0);
		if(pImg->m_External)
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, LoadFlag);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			char aTexName[128];
			str_format(aTexName, sizeof(aTexName), "%s %s", "embedded:", pName);
			m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, CImageInfo::FORMAT_RGBA, pData, CImageInfo::FORMAT_RGBA, LoadFlag, aTexName);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}
}

void CMapImages::OnMapLoad()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	CLayers *pLayers = m_pClient->Layers();
	OnMapLoadImpl(pLayers, pMap);
}

void CMapImages::LoadBackground(class CLayers *pLayers, class IMap *pMap)
{
	OnMapLoadImpl(pLayers, pMap);
}

IGraphics::CTextureHandle CMapImages::GetEntities()
{
	// DDNet default to prevent delay in seeing entities
	const char *pEntities = "ddnet";
	if(GameClient()->m_GameInfo.m_EntitiesDDNet)
		pEntities = "ddnet";
	else if(GameClient()->m_GameInfo.m_EntitiesDDRace)
		pEntities = "ddrace";
	else if(GameClient()->m_GameInfo.m_EntitiesRace)
		pEntities = "race";
	else if (GameClient()->m_GameInfo.m_EntitiesBW)
		pEntities = "blockworlds";
	else if(GameClient()->m_GameInfo.m_EntitiesFNG)
		pEntities = "fng";
	else if(GameClient()->m_GameInfo.m_EntitiesVanilla)
		pEntities = "vanilla";

	if(!m_EntitiesIsLoaded || m_pEntitiesGameType != pEntities)
	{
		char aPath[64];
		str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", pEntities);

		if(m_EntitiesTextures >= 0)
			Graphics()->UnloadTexture(m_EntitiesTextures);
		int TextureLoadFlag = Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;
		m_EntitiesTextures = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);
		m_EntitiesIsLoaded = true;
		m_pEntitiesGameType = pEntities;
	}
	return m_EntitiesTextures;
}

IGraphics::CTextureHandle CMapImages::GetSpeedupArrow()
{
	if(!m_SpeedupArrowIsLoaded)
	{
		int TextureLoadFlag = (Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE_SINGLE_LAYER : IGraphics::TEXLOAD_TO_3D_TEXTURE_SINGLE_LAYER) | IGraphics::TEXLOAD_NO_2D_TEXTURE;
		m_SpeedupArrowTexture = Graphics()->LoadTexture(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_pFilename, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureLoadFlag);

		m_SpeedupArrowIsLoaded = true;
	}

	return m_SpeedupArrowTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayBottom()
{
	return m_OverlayBottomTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayTop()
{
	return m_OverlayTopTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayCenter()
{
	return m_OverlayCenterTexture;
}

void CMapImages::SetTextureScale(int Scale)
{
	if(m_TextureScale == Scale)
		return;

	m_TextureScale = Scale;

	if(Graphics() && m_OverlayCenterTexture != -1) // check if component was initialized
	{
		// reinitialize component
		Graphics()->UnloadTexture(m_OverlayBottomTexture);
		Graphics()->UnloadTexture(m_OverlayTopTexture);
		Graphics()->UnloadTexture(m_OverlayCenterTexture);

		m_OverlayBottomTexture = IGraphics::CTextureHandle();
		m_OverlayTopTexture = IGraphics::CTextureHandle();
		m_OverlayCenterTexture = IGraphics::CTextureHandle();

		InitOverlayTextures();
	}
}

int CMapImages::GetTextureScale()
{
	return m_TextureScale;
}

IGraphics::CTextureHandle CMapImages::UploadEntityLayerText(int TextureSize, int MaxWidth, int YOffset)
{	
	void *pMem = calloc(1024 * 1024 * 4, 1);

	UpdateEntityLayerText(pMem, 4, 1024, 1024, TextureSize, MaxWidth, YOffset, 0);
	UpdateEntityLayerText(pMem, 4, 1024, 1024, TextureSize, MaxWidth, YOffset, 1);
	UpdateEntityLayerText(pMem, 4, 1024, 1024, TextureSize, MaxWidth, YOffset, 2, 255);

	int TextureLoadFlag = (Graphics()->HasTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE) | IGraphics::TEXLOAD_NO_2D_TEXTURE;
	IGraphics::CTextureHandle Texture = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_RGBA, pMem, CImageInfo::FORMAT_RGBA, TextureLoadFlag);
	free(pMem);

	return Texture;
}

void CMapImages::UpdateEntityLayerText(void* pTexBuffer, int ImageColorChannelCount, int TexWidth, int TexHeight, int TextureSize, int MaxWidth, int YOffset, int NumbersPower, int MaxNumber)
{
	char aBuf[4];
	int DigitsCount = NumbersPower+1;

	int CurrentNumber = pow(10, NumbersPower);
	
	if (MaxNumber == -1)
		MaxNumber = CurrentNumber*10-1;
	
	str_format(aBuf, 4, "%d", CurrentNumber);
	
	int CurrentNumberSuitableFontSize = TextRender()->AdjustFontSize(aBuf, DigitsCount, TextureSize, MaxWidth);
	int UniversalSuitableFontSize = CurrentNumberSuitableFontSize*0.95f; // should be smoothed enough to fit any digits combination

	int ApproximateTextWidth = TextRender()->CalculateTextWidth(aBuf, DigitsCount, 0, UniversalSuitableFontSize);
	int XOffSet = (64-ApproximateTextWidth)/2;
	YOffset += ((TextureSize - UniversalSuitableFontSize)/2);

	for (; CurrentNumber <= MaxNumber; ++CurrentNumber)
	{
		str_format(aBuf, 4, "%d", CurrentNumber);

		float x = (CurrentNumber%16)*64;
		float y = (CurrentNumber/16)*64;

		TextRender()->UploadEntityLayerText(pTexBuffer, ImageColorChannelCount, TexWidth, TexHeight, aBuf, DigitsCount, x+XOffSet, y+YOffset, UniversalSuitableFontSize);
	}
}

void CMapImages::InitOverlayTextures()
{
	int TextureSize = 64*m_TextureScale/100;
	int TextureToVerticalCenterOffset = (64-TextureSize)/2; // should be used to move texture to the center of 64 pixels area
	
	if(m_OverlayBottomTexture == -1)
	{
		m_OverlayBottomTexture = UploadEntityLayerText(TextureSize/2, 64, 32+TextureToVerticalCenterOffset/2);
	}

	if(m_OverlayTopTexture == -1)
	{
		m_OverlayTopTexture = UploadEntityLayerText(TextureSize/2, 64, TextureToVerticalCenterOffset/2);
	}

	if(m_OverlayCenterTexture == -1)
	{
		m_OverlayCenterTexture = UploadEntityLayerText(TextureSize, 64, TextureToVerticalCenterOffset);
	}
}
