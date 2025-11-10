#include "sound.h"

#include <engine/sound.h>

CEditorSound::CEditorSound(CEditorMap *pMap) :
	CMapObject(pMap)
{
}

CEditorSound::~CEditorSound()
{
	Sound()->UnloadSample(m_SoundId);
	free(m_pData);
	m_pData = nullptr;
}
