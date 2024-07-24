#ifndef GAME_EDITOR_MAPITEMS_SOUND_H
#define GAME_EDITOR_MAPITEMS_SOUND_H

#include <base/types.h>
#include <game/editor/component.h>

class CEditorSound : public CEditorComponent
{
public:
	explicit CEditorSound(CEditor *pEditor);
	~CEditorSound();

	int m_SoundId = -1;
	char m_aName[IO_MAX_PATH_LENGTH] = "";

	void *m_pData = nullptr;
	unsigned m_DataSize = 0;
};

#endif
