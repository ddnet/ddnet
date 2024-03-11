#include "sound.h"

#include <engine/sound.h>

CEditorSound::CEditorSound(CEditor *pEditor)
{
	Init(pEditor);
}

CEditorSound::~CEditorSound()
{
	Sound()->UnloadSample(m_SoundId);
	free(m_pData);
	m_pData = nullptr;
}
