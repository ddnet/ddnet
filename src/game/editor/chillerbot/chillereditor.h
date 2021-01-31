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

	void SetCursor(class CEditor *pEditor);

public:
	CChillerEditor();
	void DoMapEditor(class CEditor *pEditor, int Inside);
	void Init(class CEditor *pEditor);
};

#endif
