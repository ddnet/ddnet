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
	int64 m_NextCursorBlink;
	bool m_DrawCursor;
	IGraphics::CTextureHandle m_CursorTextTexture;
	class CEditor *m_pEditor;

	void SetCursor();

public:
	CChillerEditor();
	void DoMapEditor();
	void Init(class CEditor *pEditor);
};

#endif
