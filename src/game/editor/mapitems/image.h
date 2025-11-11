#ifndef GAME_EDITOR_MAPITEMS_IMAGE_H
#define GAME_EDITOR_MAPITEMS_IMAGE_H

#include <engine/graphics.h>

#include <game/editor/auto_map.h>
#include <game/editor/map_object.h>

class CEditorImage : public CImageInfo, public CMapObject
{
public:
	explicit CEditorImage(CEditorMap *pMap);
	~CEditorImage() override;
	void OnAttach(CEditorMap *pMap) override;

	void AnalyseTileFlags();
	void Free();

	IGraphics::CTextureHandle m_Texture;
	int m_External = 0;
	char m_aName[IO_MAX_PATH_LENGTH] = "";
	unsigned char m_aTileFlags[256];

	CAutoMapper m_AutoMapper;
};

#endif
