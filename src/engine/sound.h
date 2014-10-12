/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SOUND_H
#define ENGINE_SOUND_H

#include "kernel.h"

class ISound : public IInterface
{
	MACRO_INTERFACE("sound", 0)
public:
	enum
	{
		FLAG_LOOP=1,
		FLAG_POS=2,
		FLAG_ALL=3
	};

	class CVoiceHandle
	{
		friend class ISound;
		int m_Id;
		int m_Age;
	public:
		CVoiceHandle()
		: m_Id(-1), m_Age(-1)
		{}

		bool IsValid() const { return (Id() >= 0) && (Age() >= 0); }
		int Id() const { return m_Id; }
		int Age() const { return m_Age; }

		bool operator ==(const CVoiceHandle &Other) const { return m_Id == Other.m_Id && m_Age == Other.m_Age; }
	};


	virtual bool IsSoundEnabled() = 0;

	virtual int LoadWV(const char *pFilename) = 0;
	virtual int LoadWVFromMem(const void *pData, unsigned DataSize) = 0;
	virtual void UnloadSample(int SampleID) = 0;

	virtual float GetSampleDuration(int SampleID) = 0; // in s

	virtual void SetChannel(int ChannelID, float Volume, float Panning) = 0;
	virtual void SetListenerPos(float x, float y) = 0;

	virtual void SetVoiceVolume(CVoiceHandle Voice, float Volume) = 0;
	virtual void SetVoiceMaxDistance(CVoiceHandle Voice, int Distance) = 0;
	virtual void SetVoiceLocation(CVoiceHandle Voice, float x, float y) = 0;

	virtual CVoiceHandle PlayAt(int ChannelID, int SampleID, int Flags, float x, float y) = 0;
	virtual CVoiceHandle Play(int ChannelID, int SampleID, int Flags) = 0;
	virtual void Stop(int SampleID) = 0;
	virtual void StopAll() = 0;

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
	MACRO_INTERFACE("enginesound", 0)
public:
	virtual int Init() = 0;
	virtual int Update() = 0;
	virtual int Shutdown() = 0;
};

extern IEngineSound *CreateEngineSound();

#endif
