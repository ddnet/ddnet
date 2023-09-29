#include "sound.h"

#include <engine/sound.h>

CEditorSound::CEditorSound(CEditor *pEditor)
{
	Init(pEditor);
}

CEditorSound::~CEditorSound()
{
	Sound()->UnloadSample(m_SoundID);
	free(m_pData);
	m_pData = nullptr;
}
