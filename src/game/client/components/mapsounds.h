#ifndef GAME_CLIENT_COMPONENTS_MAPSOUNDS_H
#define GAME_CLIENT_COMPONENTS_MAPSOUNDS_H

#include <vector>

#include <engine/sound.h>

#include <game/client/component.h>

struct CSoundSource;

class CMapSounds : public CComponent
{
	int m_aSounds[64] = {0};
	int m_Count = 0;

	struct CSourceQueueEntry
	{
		int m_Sound = 0;
		bool m_HighDetail = false;
		ISound::CVoiceHandle m_Voice;
		CSoundSource *m_pSource = nullptr;

		bool operator==(const CSourceQueueEntry &Other) const { return (m_Sound == Other.m_Sound) && (m_Voice == Other.m_Voice) && (m_pSource == Other.m_pSource); }
	};

	std::vector<CSourceQueueEntry> m_vSourceQueue;

	void Clear();

public:
	CMapSounds();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnMapLoad() override;
	virtual void OnRender() override;
	virtual void OnStateChange(int NewState, int OldState) override;
};

#endif // GAME_CLIENT_COMPONENTS_MAPSOUNDS_H
