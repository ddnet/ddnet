#ifndef GAME_EDITOR_MAPITEMS_IMAGE_H
#define GAME_EDITOR_MAPITEMS_IMAGE_H

#include <engine/graphics.h>

#include <game/editor/auto_map.h>
#include <game/editor/component.h>

class CEditorImage : public CImageInfo, public CEditorComponent
{
public:
	explicit CEditorImage(CEditor *pEditor);
	~CEditorImage();

	void OnInit(CEditor *pEditor) override;
	void AnalyseTileFlags();

	IGraphics::CTextureHandle m_Texture;
	int m_External = 0;
	char m_aName[IO_MAX_PATH_LENGTH] = "";
	unsigned char m_aTileFlags[256];
	CAutoMapper m_AutoMapper;
};

#endif
