/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SOUND_H
#define ENGINE_CLIENT_SOUND_H

#include <base/lock.h>

#include <engine/sound.h>

#include <SDL_audio.h>

#include <atomic>

struct CSample
{
	int m_Index;
	int m_NextFreeSampleIndex;

	short *m_pData;
	int m_NumFrames;
	int m_Rate;
	int m_Channels;
	int m_LoopStart;
	int m_LoopEnd;
	int m_PausedAt;

	float TotalTime() const
	{
		return m_NumFrames / (float)m_Rate;
	}
};

struct CChannel
{
	int m_Vol;
	int m_Pan;
};

struct CVoice
{
	CSample *m_pSample;
	CChannel *m_pChannel;
	int m_Age; // increases when reused
	int m_Tick;
	int m_Vol; // 0 - 255
	int m_Flags;
	int m_X, m_Y;
	float m_Falloff; // [0.0, 1.0]

	int m_Shape;
	union
	{
		ISound::CVoiceShapeCircle m_Circle;
		ISound::CVoiceShapeRectangle m_Rectangle;
	};
};

class CSound : public IEngineSound
{
	enum
	{
		NUM_SAMPLES = 512,
		NUM_VOICES = 256,
		NUM_CHANNELS = 16,
	};

	bool m_SoundEnabled = false;
	SDL_AudioDeviceID m_Device = 0;
	CLock m_SoundLock;

	CSample m_aSamples[NUM_SAMPLES] = {{0}};
	int m_FirstFreeSampleIndex = 0;

	CVoice m_aVoices[NUM_VOICES] = {{0}};
	CChannel m_aChannels[NUM_CHANNELS] = {{255, 0}};
	int m_NextVoice = 0;
	uint32_t m_MaxFrames = 0;

	std::atomic<int> m_CenterX = 0;
	std::atomic<int> m_CenterY = 0;
	std::atomic<int> m_SoundVolume = 100;
	int m_MixingRate = 48000;

	class IEngineGraphics *m_pGraphics = nullptr;
	IStorage *m_pStorage = nullptr;

	int *m_pMixBuffer = nullptr;

	CSample *AllocSample();
	void RateConvert(CSample &Sample) const;

	bool DecodeOpus(CSample &Sample, const void *pData, unsigned DataSize) const;
	bool DecodeWV(CSample &Sample, const void *pData, unsigned DataSize) const;

	void UpdateVolume();

public:
	int Init() override;
	int Update() override;
	void Shutdown() override REQUIRES(!m_SoundLock);

	bool IsSoundEnabled() override { return m_SoundEnabled; }

	int LoadOpus(const char *pFilename, int StorageType = IStorage::TYPE_ALL) override REQUIRES(!m_SoundLock);
	int LoadWV(const char *pFilename, int StorageType = IStorage::TYPE_ALL) override REQUIRES(!m_SoundLock);
	int LoadOpusFromMem(const void *pData, unsigned DataSize, bool FromEditor) override REQUIRES(!m_SoundLock);
	int LoadWVFromMem(const void *pData, unsigned DataSize, bool FromEditor) override REQUIRES(!m_SoundLock);
	void UnloadSample(int SampleId) override REQUIRES(!m_SoundLock);

	float GetSampleTotalTime(int SampleId) override; // in s
	float GetSampleCurrentTime(int SampleId) override REQUIRES(!m_SoundLock); // in s
	void SetSampleCurrentTime(int SampleId, float Time) override REQUIRES(!m_SoundLock);

	void SetChannel(int ChannelId, float Vol, float Pan) override;
	void SetListenerPos(float x, float y) override;

	void SetVoiceVolume(CVoiceHandle Voice, float Volume) override REQUIRES(!m_SoundLock);
	void SetVoiceFalloff(CVoiceHandle Voice, float Falloff) override REQUIRES(!m_SoundLock);
	void SetVoiceLocation(CVoiceHandle Voice, float x, float y) override REQUIRES(!m_SoundLock);
	void SetVoiceTimeOffset(CVoiceHandle Voice, float TimeOffset) override REQUIRES(!m_SoundLock); // in s

	void SetVoiceCircle(CVoiceHandle Voice, float Radius) override REQUIRES(!m_SoundLock);
	void SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height) override REQUIRES(!m_SoundLock);

	CVoiceHandle Play(int ChannelId, int SampleId, int Flags, float x, float y) REQUIRES(!m_SoundLock);
	CVoiceHandle PlayAt(int ChannelId, int SampleId, int Flags, float x, float y) override REQUIRES(!m_SoundLock);
	CVoiceHandle Play(int ChannelId, int SampleId, int Flags) override REQUIRES(!m_SoundLock);
	void Pause(int SampleId) override REQUIRES(!m_SoundLock);
	void Stop(int SampleId) override REQUIRES(!m_SoundLock);
	void StopAll() override REQUIRES(!m_SoundLock);
	void StopVoice(CVoiceHandle Voice) override REQUIRES(!m_SoundLock);
	bool IsPlaying(int SampleId) override REQUIRES(!m_SoundLock);

	void Mix(short *pFinalOut, unsigned Frames) override REQUIRES(!m_SoundLock);
	void PauseAudioDevice() override;
	void UnpauseAudioDevice() override;
};

#endif
