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

CMapImages::CMapImages()
{
	m_Count = 0;
	m_EntitiesTextures = -1;
	m_OverlayBottomTexture = -1;
	m_OverlayTopTexture = -1;
	m_OverlayCenterTexture = -1;
}

void CMapImages::OnInit()
{
	//TODO: improve this a bit -- with better fron sizes etc.
	if(m_OverlayBottomTexture == -1)
	{
		void *pMem = mem_alloc(1024*1024, 1);
		mem_zero(pMem, 1024*1024);
		m_OverlayBottomTexture = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);
		mem_free(pMem);
		
		for(int i = 0; i < 256; ++i)
		{
			char buff[4];
			str_format(buff, 4, "%d", i);
			int len = str_length(buff);
			
			float x = (i%16) * 64;
			float y = (int)(i/16)* 64;
			TextRender()->UploadText(m_OverlayBottomTexture, buff, -1, x+1, y + 12 + 32, 20, 64-1);
		}
	}
	if(m_OverlayTopTexture == -1)
	{
		void *pMem = mem_alloc(1024*1024, 1);
		mem_zero(pMem, 1024*1024);
		m_OverlayTopTexture = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);
		mem_free(pMem);
		
		for(int i = 0; i < 256; ++i)
		{
			char buff[4];
			str_format(buff, 4, "%d", i);
			int len = str_length(buff);
			
			float x = (i%16) * 64;
			float y = (int)(i/16)* 64;
			TextRender()->UploadText(m_OverlayTopTexture, buff, -1, x+1, y+1, 20, 64-1);
		}
	}
	if(m_OverlayCenterTexture == -1)
	{
		void *pMem = mem_alloc(1024*1024, 1);
		mem_zero(pMem, 1024*1024);
		m_OverlayCenterTexture = Graphics()->LoadTextureRaw(1024, 1024, CImageInfo::FORMAT_ALPHA, pMem, CImageInfo::FORMAT_ALPHA, IGraphics::TEXLOAD_NOMIPMAPS);
		mem_free(pMem);

		for(int i = 0; i < 256; ++i)
		{
			char buff[4];
			str_format(buff, 4, "%d", i);
			int len = str_length(buff);
			
			float x = (i%16) * 64;
			float y = (int)(i/16)* 64;
			
			int MinSize = (len == 3 ? 20 : 50);
			int OffY = (len == 3 ? 10 : 5);
			int OffX = (len == 3 ? 5 : 1);
			
			TextRender()->UploadText(m_OverlayCenterTexture, buff, -1, x + OffX, y + OffY, -1, 64-(OffX*2), 64, MinSize);
		}
	}
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
	CServerInfo Info;
	Client()->GetServerInfo(&Info);
	
	if(m_EntitiesTextures == -1 || str_comp(m_aEntitiesGameType, Info.m_aGameType))
	{
		// DDNet default to prevent delay in seeing entities
		char file[64] = "ddnet";
		if(IsDDNet(&Info))
			str_copy(file, "ddnet", sizeof(file));
		else if(IsDDRace(&Info))
			str_copy(file, "ddrace", sizeof(file));
		else if(IsRace(&Info))
			str_copy(file, "race", sizeof(file));
		else if(IsFNG(&Info))
			str_copy(file, "fng", sizeof(file));
		else if(IsVanilla(&Info))
			str_copy(file, "vanilla", sizeof(file));

		char path[64];
		str_format(path, sizeof(path), "editor/entities_clear/%s.png", file);
		
		if(m_EntitiesTextures >= 0)
			Graphics()->UnloadTexture(m_EntitiesTextures);
		m_EntitiesTextures = Graphics()->LoadTexture(path, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

		str_copy(m_aEntitiesGameType, Info.m_aGameType, sizeof(m_aEntitiesGameType));
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
