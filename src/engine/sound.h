/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SOUND_H
#define ENGINE_SOUND_H

#include <engine/kernel.h>
#include <engine/storage.h>

#include <base/vmath.h>

class ISound : public IInterface
{
	MACRO_INTERFACE("sound")
public:
	enum
	{
		FLAG_LOOP = 1 << 0,
		FLAG_POS = 1 << 1,
		FLAG_NO_PANNING = 1 << 2,
		FLAG_PREVIEW = 1 << 3,
		FLAG_ALL = FLAG_LOOP | FLAG_POS | FLAG_NO_PANNING | FLAG_PREVIEW,
	};

	enum
	{
		SHAPE_CIRCLE,
		SHAPE_RECTANGLE,
	};

	// unused
	struct CSampleHandle
	{
		int m_SampleId;
	};

	struct CVoiceShapeCircle
	{
		float m_Radius;
	};

	struct CVoiceShapeRectangle
	{
		float m_Width;
		float m_Height;
	};

	class CVoiceHandle
	{
		friend class ISound;
		int m_Id;
		int m_Age;

	public:
		CVoiceHandle() :
			m_Id(-1), m_Age(-1)
		{
		}

		bool IsValid() const { return (Id() >= 0) && (Age() >= 0); }
		int Id() const { return m_Id; }
		int Age() const { return m_Age; }

		bool operator==(const CVoiceHandle &Other) const { return m_Id == Other.m_Id && m_Age == Other.m_Age; }
	};

	virtual bool IsSoundEnabled() = 0;

	virtual int LoadOpus(const char *pFilename, int StorageType = IStorage::TYPE_ALL) = 0;
	virtual int LoadWV(const char *pFilename, int StorageType = IStorage::TYPE_ALL) = 0;
	virtual int LoadOpusFromMem(const void *pData, unsigned DataSize, bool FromEditor = false) = 0;
	virtual int LoadWVFromMem(const void *pData, unsigned DataSize, bool FromEditor = false) = 0;
	virtual void UnloadSample(int SampleId) = 0;

	virtual float GetSampleTotalTime(int SampleId) = 0; // in s
	virtual float GetSampleCurrentTime(int SampleId) = 0; // in s
	virtual void SetSampleCurrentTime(int SampleId, float Time) = 0;

	virtual void SetChannel(int ChannelId, float Volume, float Panning) = 0;
	virtual void SetListenerPosition(vec2 Position) = 0;

	virtual void SetVoiceVolume(CVoiceHandle Voice, float Volume) = 0;
	virtual void SetVoiceFalloff(CVoiceHandle Voice, float Falloff) = 0;
	virtual void SetVoicePosition(CVoiceHandle Voice, vec2 Position) = 0;
	virtual void SetVoiceTimeOffset(CVoiceHandle Voice, float TimeOffset) = 0; // in s

	virtual void SetVoiceCircle(CVoiceHandle Voice, float Radius) = 0;
	virtual void SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height) = 0;

	virtual CVoiceHandle PlayAt(int ChannelId, int SampleId, int Flags, float Volume, vec2 Position) = 0;
	virtual CVoiceHandle Play(int ChannelId, int SampleId, int Flags, float Volume) = 0;
	virtual void Pause(int SampleId) = 0;
	virtual void Stop(int SampleId) = 0;
	virtual void StopAll() = 0;
	virtual void StopVoice(CVoiceHandle Voice) = 0;
	virtual bool IsPlaying(int SampleId) = 0;

	virtual int MixingRate() const = 0;
	virtual void Mix(short *pFinalOut, unsigned Frames) = 0;

	// useful for thread synchronization
	virtual void PauseAudioDevice() = 0;
	virtual void UnpauseAudioDevice() = 0;

protected:
	inline CVoiceHandle CreateVoiceHandle(int Index, int Age)
	{
		CVoiceHandle Voice;
		Voice.m_Id = Index;
		Voice.m_Age = Age;
		return Voice;
	}
};

class IEngineSound : public ISound
{
	MACRO_INTERFACE("enginesound")
public:
	virtual int Init() = 0;
	virtual int Update() = 0;
	virtual void Shutdown() override = 0;
};

extern IEngineSound *CreateEngineSound();

#endif
