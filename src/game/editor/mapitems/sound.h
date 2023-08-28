#ifndef GAME_EDITOR_MAPITEMS_SOUND_H
#define GAME_EDITOR_MAPITEMS_SOUND_H

#include <base/system.h>

class CEditor;

class CEditorSound
{
public:
	CEditor *m_pEditor;

	CEditorSound(CEditor *pEditor)
	{
		m_pEditor = pEditor;
		m_aName[0] = 0;
		m_SoundID = 0;

		m_pData = nullptr;
		m_DataSize = 0;
	}

	~CEditorSound();

	int m_SoundID;
	char m_aName[IO_MAX_PATH_LENGTH];

	void *m_pData;
	unsigned m_DataSize;
};

#endif
