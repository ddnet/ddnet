#include "sound.h"

#include <engine/sound.h>

#include <game/editor/editor.h>

CEditorSound::~CEditorSound()
{
	m_pEditor->Sound()->UnloadSample(m_SoundID);
	free(m_pData);
	m_pData = nullptr;
}
