#ifndef GAME_EDITOR_MAPITEMS_IMAGE_H
#define GAME_EDITOR_MAPITEMS_IMAGE_H

#include <engine/graphics.h>

#include <game/editor/auto_map.h>

class CEditor;

class CEditorImage : public CImageInfo
{
public:
	CEditor *m_pEditor;

	CEditorImage(CEditor *pEditor): m_AutoMapper(pEditor)
	{
		m_pEditor = pEditor;
		m_aName[0] = 0;
		m_Texture.Invalidate();
		m_External = 0;
		m_Width = 0;
		m_Height = 0;
		m_pData = nullptr;
		m_Format = 0;
	}

	~CEditorImage();

	void AnalyseTileFlags();

	IGraphics::CTextureHandle m_Texture;
	int m_External;
	char m_aName[IO_MAX_PATH_LENGTH];
	unsigned char m_aTileFlags[256];
	class CAutoMapper m_AutoMapper;
};

#endif
