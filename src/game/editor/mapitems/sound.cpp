#include "sound.h"

#include "map.h"

#include <engine/sound.h>

CEditorSound::CEditorSound(CEditorMap *pMap, std::uint64_t RetainedId) :
	CMapObject(pMap)
{
	m_Id = RetainedId ? RetainedId : Map()->AllocateObjectId();
}

CEditorSound::~CEditorSound()
{
	Sound()->UnloadSample(m_SoundId);
}

void CEditorSound::OnAttach(CEditorMap *pMap)
{
	if(Map() != pMap)
	{
		m_Id = pMap->AllocateObjectId();
	}
	CMapObject::OnAttach(pMap);
}
