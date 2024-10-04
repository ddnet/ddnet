#ifndef GAME_CLIENT_COMPONENTS_MAPSOUNDS_H
#define GAME_CLIENT_COMPONENTS_MAPSOUNDS_H

#include <vector>

#include <engine/sound.h>

#include <game/client/component.h>
#include <game/mapitems.h>

struct CSoundSource;

class CMapSounds : public CComponent
{
	int m_aSounds[MAX_MAPSOUNDS];
	int m_Count;

	struct CSourceQueueEntry
	{
		int m_Sound;
		bool m_HighDetail;
		ISound::CVoiceHandle m_Voice;
		CSoundSource *m_pSource;

		bool operator==(const CSourceQueueEntry &Other) const { return (m_Sound == Other.m_Sound) && (m_Voice == Other.m_Voice) && (m_pSource == Other.m_pSource); }
	};

	std::vector<CSourceQueueEntry> m_vSourceQueue;

	void Clear();

public:
	CMapSounds();
	virtual int Sizeof() const override { return sizeof(*this); }

	void Play(int Channel, int SoundId);
	void PlayAt(int Channel, int SoundId, vec2 Position);

	virtual void OnMapLoad() override;
	virtual void OnRender() override;
	virtual void OnStateChange(int NewState, int OldState) override;
};

#endif // GAME_CLIENT_COMPONENTS_MAPSOUNDS_H
