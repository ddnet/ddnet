#ifndef GAME_EDITOR_MAPITEMS_SOUND_H
#define GAME_EDITOR_MAPITEMS_SOUND_H

#include "document_values.h"

#include <base/types.h>

#include <game/editor/map_object.h>

class CEditorSound : public CMapObject, public CEditorSoundValues
{
public:
	void OnAttach(CEditorMap *pMap) override;
	explicit CEditorSound(CEditorMap *pMap, std::uint64_t RetainedId = 0);
	CEditorSound(const CEditorSound &) = delete;
	CEditorSound &operator=(const CEditorSound &) = delete;
	~CEditorSound() override;

	int m_SoundId = -1;
};

#endif
