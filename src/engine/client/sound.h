/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SOUND_H
#define ENGINE_CLIENT_SOUND_H

#include <engine/sound.h>

#include <SDL_audio.h>

#include <atomic>
#include <mutex>

struct CSample
{
	short *m_pData;
	int m_NumFrames;
	int m_Rate;
	int m_Channels;
	int m_LoopStart;
	int m_LoopEnd;
	int m_PausedAt;
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
	std::mutex m_SoundLock;

	CSample m_aSamples[NUM_SAMPLES] = {{0}};
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

	int AllocID();
	void RateConvert(CSample &Sample);

	bool DecodeOpus(CSample &Sample, const void *pData, unsigned DataSize);
	bool DecodeWV(CSample &Sample, const void *pData, unsigned DataSize);

	void UpdateVolume();

public:
	int Init() override;
	int Update() override;
	void Shutdown() override;

	bool IsSoundEnabled() override { return m_SoundEnabled; }

	int LoadOpus(const char *pFilename, int StorageType = IStorage::TYPE_ALL) override;
	int LoadWV(const char *pFilename, int StorageType = IStorage::TYPE_ALL) override;
	int LoadOpusFromMem(const void *pData, unsigned DataSize, bool FromEditor) override;
	int LoadWVFromMem(const void *pData, unsigned DataSize, bool FromEditor) override;
	void UnloadSample(int SampleID) override;

	float GetSampleDuration(int SampleID) override; // in s

	void SetChannel(int ChannelID, float Vol, float Pan) override;
	void SetListenerPos(float x, float y) override;

	void SetVoiceVolume(CVoiceHandle Voice, float Volume) override;
	void SetVoiceFalloff(CVoiceHandle Voice, float Falloff) override;
	void SetVoiceLocation(CVoiceHandle Voice, float x, float y) override;
	void SetVoiceTimeOffset(CVoiceHandle Voice, float TimeOffset) override; // in s

	void SetVoiceCircle(CVoiceHandle Voice, float Radius) override;
	void SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height) override;

	CVoiceHandle Play(int ChannelID, int SampleID, int Flags, float x, float y);
	CVoiceHandle PlayAt(int ChannelID, int SampleID, int Flags, float x, float y) override;
	CVoiceHandle Play(int ChannelID, int SampleID, int Flags) override;
	void Stop(int SampleID) override;
	void StopAll() override;
	void StopVoice(CVoiceHandle Voice) override;
	bool IsPlaying(int SampleID) override;

	void Mix(short *pFinalOut, unsigned Frames) override;
	void PauseAudioDevice() override;
	void UnpauseAudioDevice() override;
};

#endif
