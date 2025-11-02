#ifndef GAME_EDITOR_EDITOR_ACTION_H
#define GAME_EDITOR_EDITOR_ACTION_H

#include <game/editor/map_object.h>

class IEditorAction : public CMapObject
{
public:
	IEditorAction(CEditorMap *pMap) :
		CMapObject(pMap) {}

	virtual void Undo() = 0;
	virtual void Redo() = 0;

	virtual bool IsEmpty() { return false; }

	const char *DisplayText() const { return m_aDisplayText; }

protected:
	char m_aDisplayText[256];
};

#endif
