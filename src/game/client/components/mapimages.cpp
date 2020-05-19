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
}

void CMapImages::OnInit()
{
	InitOverlayTextures();
}

void CMapImages::OnMapLoad()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();

	// unload all textures
	for(int i = 0; i < m_Count; i++)
	{
		Graphics()->UnloadTexture(m_aTextures[i]);
		m_aTextures[i] = IGraphics::CTextureHandle();
	}
	m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);

	// load new textures
	for(int i = 0; i < m_Count; i++)
	{
		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start+i, 0, 0);
		if(pImg->m_External)
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, CImageInfo::FORMAT_RGBA, pData, CImageInfo::FORMAT_RGBA, 0);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}
}

void CMapImages::LoadBackground(class IMap *pMap)
{
	// unload all textures
	for(int i = 0; i < m_Count; i++)
	{
		Graphics()->UnloadTexture(m_aTextures[i]);
		m_aTextures[i] = IGraphics::CTextureHandle();
	}
	m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);

	// load new textures
	for(int i = 0; i < m_Count; i++)
	{
		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start+i, 0, 0);
		if(pImg->m_External)
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, CImageInfo::FORMAT_RGBA, pData, CImageInfo::FORMAT_RGBA, 0);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}
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
		m_EntitiesTextures = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
		m_EntitiesIsLoaded = true;
		m_pEntitiesGameType = pEntities;
	}
	return m_EntitiesTextures;
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

		InitOverlayTextures();
	}
}

int CMapImages::GetTextureScale()
{
	return m_TextureScale;
}

IGraphics::CTextureHandle CMapImages::UploadEntityLayerText(int TextureSize, int YOffset)
{	
	void *pMem = calloc(1024 * 1024, 1);
	IGraphics::CTextureHandle Texture = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);
	free(pMem);

	UpdateEntityLayerText(Texture, TextureSize, YOffset, 0);
	UpdateEntityLayerText(Texture, TextureSize, YOffset, 1);
	UpdateEntityLayerText(Texture, TextureSize, YOffset, 2, 255);

	return Texture;
}

void CMapImages::UpdateEntityLayerText(IGraphics::CTextureHandle Texture, int TextureSize, int YOffset, int NumbersPower, int MaxNumber)
{
	char aBuf[4];
	int DigitsCount = NumbersPower+1;

	int CurrentNumber = pow(10, NumbersPower);
	
	if (MaxNumber == -1)
		MaxNumber = CurrentNumber*10-1;
	
	str_format(aBuf, 4, "%d", CurrentNumber);
	
	int CurrentNumberSuitableFontSize = TextRender()->AdjustFontSize(aBuf, DigitsCount, TextureSize);
	int UniversalSuitableFontSize = CurrentNumberSuitableFontSize*0.9; // should be smoothed enough to fit any digits combination

	if (UniversalSuitableFontSize < 1)
	{
		dbg_msg("pFont", "texture with id '%d' will not be loaded. Reason - font is too small", (int)Texture);
	}

	int ApproximateTextWidth = TextRender()->CalculateTextWidth(aBuf, DigitsCount, 0, UniversalSuitableFontSize);
	int XOffSet = (64-ApproximateTextWidth)/2;
	YOffset += ((TextureSize - UniversalSuitableFontSize)/2);

	for (; CurrentNumber <= MaxNumber; ++CurrentNumber)
	{
		str_format(aBuf, 4, "%d", CurrentNumber);

		float x = (CurrentNumber%16)*64;
		float y = (CurrentNumber/16)*64;

		TextRender()->UploadEntityLayerText(Texture, aBuf, DigitsCount, x+XOffSet, y+YOffset, UniversalSuitableFontSize);
	}
}

void CMapImages::InitOverlayTextures()
{
	int TextureSize = 64*m_TextureScale/100;
	int TextureToVerticalCenterOffset = (64-TextureSize)/2; // should be used to move texture to the center of 64 pixels area
	
	if(m_OverlayBottomTexture == -1)
	{
		m_OverlayBottomTexture = UploadEntityLayerText(TextureSize/2, 32+TextureToVerticalCenterOffset/2);
	}

	if(m_OverlayTopTexture == -1)
	{
		m_OverlayTopTexture = UploadEntityLayerText(TextureSize/2, TextureToVerticalCenterOffset/2);
	}

	if(m_OverlayCenterTexture == -1)
	{
		m_OverlayCenterTexture = UploadEntityLayerText(TextureSize, TextureToVerticalCenterOffset);
	}
}
