/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SOUND_H
#define ENGINE_CLIENT_SOUND_H

#include <engine/shared/video.h>
#include <engine/sound.h>

#include <SDL_audio.h>

class IEngineGraphics;
class IStorage;

class CSound : public IEngineSound
{
	bool m_SoundEnabled;
	SDL_AudioDeviceID m_Device;

	IEngineGraphics *m_pGraphics;
	IStorage *m_pStorage;

	int AllocID();

	static void RateConvert(int SampleID);

	// TODO: Refactor: clean this mess up
	static int DecodeWV(int SampleID, const void *pData, unsigned DataSize);
	static int DecodeOpus(int SampleID, const void *pData, unsigned DataSize);

public:
	int Init() override;
	int Update() override;
	void Shutdown() override;

	bool IsSoundEnabled() override { return m_SoundEnabled; }

	int LoadWV(const char *pFilename) override;
	int LoadWVFromMem(const void *pData, unsigned DataSize, bool FromEditor) override;
	int LoadOpus(const char *pFilename) override;
	int LoadOpusFromMem(const void *pData, unsigned DataSize, bool FromEditor) override;
	void UnloadSample(int SampleID) override;

	float GetSampleDuration(int SampleID) override; // in s

	void SetListenerPos(float x, float y) override;
	void SetChannel(int ChannelID, float Vol, float Pan) override;

	void SetVoiceVolume(CVoiceHandle Voice, float Volume) override;
	void SetVoiceFalloff(CVoiceHandle Voice, float Falloff) override;
	void SetVoiceLocation(CVoiceHandle Voice, float x, float y) override;
	void SetVoiceTimeOffset(CVoiceHandle Voice, float offset) override; // in s

	void SetVoiceCircle(CVoiceHandle Voice, float Radius) override;
	void SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height) override;

	CVoiceHandle Play(int ChannelID, int SampleID, int Flags, float x, float y);
	CVoiceHandle PlayAt(int ChannelID, int SampleID, int Flags, float x, float y) override;
	CVoiceHandle Play(int ChannelID, int SampleID, int Flags) override;
	void Stop(int SampleID) override;
	void StopAll() override;
	void StopVoice(CVoiceHandle Voice) override;
	bool IsPlaying(int SampleID) override;

	ISoundMixFunc GetSoundMixFunc() override;
	void PauseAudioDevice() override;
	void UnpauseAudioDevice() override;
};

#endif
