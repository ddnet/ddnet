#include "image.h"

#include <game/mapitems.h>

CEditorImage::CEditorImage(CEditor *pEditor) :
	m_AutoMapper(pEditor)
{
	OnInit(pEditor);
	m_Texture.Invalidate();
}

CEditorImage::~CEditorImage()
{
	Graphics()->UnloadTexture(&m_Texture);
	free(m_pData);
	m_pData = nullptr;
}

void CEditorImage::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);
	RegisterSubComponent(m_AutoMapper);
	InitSubComponents();
}

void CEditorImage::AnalyseTileFlags()
{
	mem_zero(m_aTileFlags, sizeof(m_aTileFlags));

	size_t TileWidth = m_Width / 16;
	size_t TileHeight = m_Height / 16;
	if(TileWidth == TileHeight && m_Format == CImageInfo::FORMAT_RGBA)
	{
		int TileId = 0;
		for(size_t ty = 0; ty < 16; ty++)
			for(size_t tx = 0; tx < 16; tx++, TileId++)
			{
				bool Opaque = true;
				for(size_t x = 0; x < TileWidth; x++)
					for(size_t y = 0; y < TileHeight; y++)
					{
						size_t p = (ty * TileWidth + y) * m_Width + tx * TileWidth + x;
						if(m_pData[p * 4 + 3] < 250)
						{
							Opaque = false;
							break;
						}
					}

				if(Opaque)
					m_aTileFlags[TileId] |= TILEFLAG_OPAQUE;
			}
	}
}

void CEditorImage::Free()
{
	Graphics()->UnloadTexture(&m_Texture);
	m_AutoMapper.Unload();
	CImageInfo::Free();
}
