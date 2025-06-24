#include "sound.h"

#include <engine/sound.h>

CEditorSound::CEditorSound(CEditor *pEditor)
{
	OnInit(pEditor);
}

CEditorSound::~CEditorSound()
{
	Sound()->UnloadSample(m_SoundId);
	free(m_pData);
	m_pData = nullptr;
}
