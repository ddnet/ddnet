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
	m_EntitiesTextures = -1;
	m_OverlayBottomTexture = -1;
	m_OverlayTopTexture = -1;
	m_OverlayCenterTexture = -1;
	m_TextureScale = TextureSize;
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
		m_aTextures[i] = -1;
	}
	m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);

	// load new textures
	for(int i = 0; i < m_Count; i++)
	{
		m_aTextures[i] = 0;

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
		m_aTextures[i] = -1;
	}
	m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);

	// load new textures
	for(int i = 0; i < m_Count; i++)
	{
		m_aTextures[i] = 0;

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

int CMapImages::GetEntities()
{
	// DDNet default to prevent delay in seeing entities
	const char *pEntities = "ddnet";
	if(GameClient()->m_GameInfo.m_EntitiesDDNet)
		pEntities = "ddnet";
	else if(GameClient()->m_GameInfo.m_EntitiesDDRace)
		pEntities = "ddrace";
	else if(GameClient()->m_GameInfo.m_EntitiesRace)
		pEntities = "race";
	else if(GameClient()->m_GameInfo.m_EntitiesFNG)
		pEntities = "fng";
	else if(GameClient()->m_GameInfo.m_EntitiesVanilla)
		pEntities = "vanilla";

	if(m_EntitiesTextures == -1 || m_pEntitiesGameType != pEntities)
	{
		char aPath[64];
		str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", pEntities);

		if(m_EntitiesTextures >= 0)
			Graphics()->UnloadTexture(m_EntitiesTextures);
		m_EntitiesTextures = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

		m_pEntitiesGameType = pEntities;
	}
	return m_EntitiesTextures;
}

int CMapImages::GetOverlayBottom()
{
	return m_OverlayBottomTexture;
}

int CMapImages::GetOverlayTop()
{
	return m_OverlayTopTexture;
}

int CMapImages::GetOverlayCenter()
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
		m_OverlayBottomTexture = m_OverlayTopTexture = m_OverlayCenterTexture = -1;

		InitOverlayTextures();
	}
}

int CMapImages::GetTextureScale()
{
	return m_TextureScale;
}

int CMapImages::UploadEntityLayerText(int TextureSize, int YOffset)
{	
	void *pMem = calloc(1024 * 1024, 1);
	int TextureID = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);
	free(pMem);

	char aBuf[4];
	int Len = str_format(aBuf, 4, "%d", 255);

	int FontSize = TextRender()->AdjustFontSize(aBuf, Len, TextureSize);

	if (FontSize < 1)
	{
		dbg_msg("pFont", "texture with id '%d' will not be loaded. Reason - font is too small", TextureID);
		return TextureID;
	}

	YOffset += ((TextureSize - FontSize)/2);

	for(int i = 0; i < 256; ++i)
	{
		int Len = str_format(aBuf, 4, "%d", i);

		float x = (i%16)*64;
		float y = (i/16)*64;
		
		TextRender()->UploadEntityLayerText(TextureID, aBuf, Len, x, y + YOffset, FontSize);
	}

	return TextureID;
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
