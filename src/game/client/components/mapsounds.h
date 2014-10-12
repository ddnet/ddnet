
#pragma once

#include <base/tl/array.h>

#include <engine/sound.h>

#include <game/client/component.h>

class CMapSounds : public CComponent
{
	int m_aSounds[64];
	int m_Count;

	struct CSourceQueueEntry
	{
		int m_Sound;
		CSoundSource *m_pSource;

		bool operator ==(const CSourceQueueEntry &Other) const { return (m_Sound == Other.m_Sound) && (m_pSource == Other.m_pSource); }
	};

	array<CSourceQueueEntry> m_lSourceQueue;

	struct CSourceVoice
	{
		ISound::CVoiceHandle m_Voice;
		CSoundSource *m_pSource;
	};
	array<CSourceVoice> m_lVoices;

public:
	CMapSounds();

	virtual void OnMapLoad();
	virtual void OnRender();
};