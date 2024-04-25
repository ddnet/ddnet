#ifndef GAME_EDITOR_EDITOR_ACTION_H
#define GAME_EDITOR_EDITOR_ACTION_H

#include <string>

class CEditor;

class IEditorAction
{
public:
	IEditorAction(CEditor *pEditor) :
		m_pEditor(pEditor) {}

	IEditorAction() = default;

	virtual ~IEditorAction() = default;

	virtual void Undo() = 0;
	virtual void Redo() = 0;

	virtual bool IsEmpty() { return false; }

	const char *DisplayText() const { return m_aDisplayText; }

protected:
	CEditor *m_pEditor;
	char m_aDisplayText[256];
};

#endif
