#ifndef GAME_EDITOR_CHILLERBOT_CHILLEREDITOR_H
#define GAME_EDITOR_CHILLERBOT_CHILLEREDITOR_H

#include <engine/graphics.h>

class CChillerEditor
{
	enum
	{
		CE_MODE_NONE,
		CE_MODE_TEXT,
	};
	int m_EditorMode;
	int m_TextIndexX;
	int m_TextIndexY;
	int m_TextLineLen;
	int m_LetterOffset;
	int m_NumberOffset;

	bool m_DrawCursor;

	int64_t m_NextCursorBlink;

	IGraphics::CTextureHandle m_CursorTextTexture;

	class CEditor *m_pEditor;
	class CLayerTiles *m_pLastLayer;

	void SetCursor();
	void ExitTextMode();
	void LoadMapresMetaFile(const char *pImage);

	void GetLayerByTile();

public:
	CChillerEditor();
	void DoMapEditor();
	void Init(class CEditor *pEditor);
};

#endif
