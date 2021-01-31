#ifndef GAME_EDITOR_CHILLERBOT_CHILLEREDITOR_H
#define GAME_EDITOR_CHILLERBOT_CHILLEREDITOR_H

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

public:
	CChillerEditor();
	void DoMapEditor(class CEditor *pEditor, int Inside);
};

#endif
