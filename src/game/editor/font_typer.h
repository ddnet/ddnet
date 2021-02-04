#ifndef GAME_EDITOR_FONT_TYPER_H
#define GAME_EDITOR_FONT_TYPER_H

#include <engine/graphics.h>

class CFontTyper
{
	enum
	{
		FT_MODE_NONE,
		FT_MODE_TEXT,
	};
	int m_EditorMode;
	int m_TextIndexX;
	int m_TextIndexY;
	int m_TextLineLen;
	int m_LetterOffset;
	int m_NumberOffset;

	bool m_DrawCursor;

	int64 m_NextCursorBlink;

	IGraphics::CTextureHandle m_CursorTextTexture;

	class CEditor *m_pEditor;
	class CLayerTiles *m_pLastLayer;

	void SetCursor();
	void ExitTextMode();
	void LoadMapresMetaFile(const char *pImage);

public:
	CFontTyper();
	void DoMapEditor();
	void Init(class CEditor *pEditor);
};

#endif
