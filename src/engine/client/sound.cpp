/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <SDL.h>

#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "sound.h"

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif
extern "C" {
#include <opusfile.h>
#include <wavpack.h>
}

#include <cmath>

static constexpr int SAMPLE_INDEX_USED = -2;
static constexpr int SAMPLE_INDEX_FULL = -1;

void CSound::Mix(short *pFinalOut, unsigned Frames)
{
	Frames = minimum(Frames, m_MaxFrames);
	mem_zero(m_pMixBuffer, Frames * 2 * sizeof(int));

	// acquire lock while we are mixing
	m_SoundLock.lock();

	const int MasterVol = m_SoundVolume.load(std::memory_order_relaxed);

	for(auto &Voice : m_aVoices)
	{
		if(!Voice.m_pSample)
			continue;

		// mix voice
		int *pOut = m_pMixBuffer;

		const int Step = Voice.m_pSample->m_Channels; // setup input sources
		short *pInL = &Voice.m_pSample->m_pData[Voice.m_Tick * Step];
		short *pInR = &Voice.m_pSample->m_pData[Voice.m_Tick * Step + 1];

		unsigned End = Voice.m_pSample->m_NumFrames - Voice.m_Tick;

		int VolumeR = round_truncate(Voice.m_pChannel->m_Vol * (Voice.m_Vol / 255.0f));
		int VolumeL = VolumeR;

		// make sure that we don't go outside the sound data
		if(Frames < End)
			End = Frames;

		// check if we have a mono sound
		if(Voice.m_pSample->m_Channels == 1)
			pInR = pInL;

		// volume calculation
		if(Voice.m_Flags & ISound::FLAG_POS && Voice.m_pChannel->m_Pan)
		{
			// TODO: we should respect the channel panning value
			const vec2 Delta = Voice.m_Position - vec2(m_ListenerPositionX.load(std::memory_order_relaxed), m_ListenerPositionY.load(std::memory_order_relaxed));
			vec2 Falloff = vec2(0.0f, 0.0f);

			float RangeX = 0.0f; // for panning
			bool InVoiceField = false;

			switch(Voice.m_Shape)
			{
			case ISound::SHAPE_CIRCLE:
			{
				const float Radius = Voice.m_Circle.m_Radius;
				RangeX = Radius;

				const float Dist = length(Delta);
				if(Dist < Radius)
				{
					InVoiceField = true;

					// falloff
					const float FalloffDistance = Radius * Voice.m_Falloff;
					Falloff.x = Falloff.y = Dist > FalloffDistance ? (Radius - Dist) / (Radius - FalloffDistance) : 1.0f;
				}
				break;
			}

			case ISound::SHAPE_RECTANGLE:
			{
				const vec2 AbsoluteDelta = vec2(absolute(Delta.x), absolute(Delta.y));
				const float w = Voice.m_Rectangle.m_Width / 2.0f;
				const float h = Voice.m_Rectangle.m_Height / 2.0f;
				RangeX = w;

				if(AbsoluteDelta.x < w && AbsoluteDelta.y < h)
				{
					InVoiceField = true;

					// falloff
					const vec2 FalloffDistance = vec2(w, h) * Voice.m_Falloff;
					Falloff.x = AbsoluteDelta.x > FalloffDistance.x ? (w - AbsoluteDelta.x) / (w - FalloffDistance.x) : 1.0f;
					Falloff.y = AbsoluteDelta.y > FalloffDistance.y ? (h - AbsoluteDelta.y) / (h - FalloffDistance.y) : 1.0f;
				}
				break;
			}
			};

			if(InVoiceField)
			{
				// panning
				if(!(Voice.m_Flags & ISound::FLAG_NO_PANNING))
				{
					if(Delta.x > 0)
						VolumeL = ((RangeX - absolute(Delta.x)) * VolumeL) / RangeX;
					else
						VolumeR = ((RangeX - absolute(Delta.x)) * VolumeR) / RangeX;
				}

				{
					VolumeL *= Falloff.x * Falloff.y;
					VolumeR *= Falloff.x * Falloff.y;
				}
			}
			else
			{
				VolumeL = 0;
				VolumeR = 0;
			}
		}

		// process all frames
		for(unsigned s = 0; s < End; s++)
		{
			*pOut++ += (*pInL) * VolumeL;
			*pOut++ += (*pInR) * VolumeR;
			pInL += Step;
			pInR += Step;
			Voice.m_Tick++;
		}

		// free voice if not used any more
		if(Voice.m_Tick == Voice.m_pSample->m_NumFrames)
		{
			if(Voice.m_Flags & ISound::FLAG_LOOP)
				Voice.m_Tick = 0;
			else
			{
				Voice.m_pSample = nullptr;
				Voice.m_Age++;
			}
		}
	}

	m_SoundLock.unlock();

	// clamp accumulated values
	for(unsigned i = 0; i < Frames * 2; i++)
		pFinalOut[i] = clamp<int>(((m_pMixBuffer[i] * MasterVol) / 101) >> 8, std::numeric_limits<short>::min(), std::numeric_limits<short>::max());

#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(pFinalOut, sizeof(short), Frames * 2);
#endif
}

static void SdlCallback(void *pUser, Uint8 *pStream, int Len)
{
	CSound *pSound = static_cast<CSound *>(pUser);

#if defined(CONF_VIDEORECORDER)
	if(!(IVideo::Current() && g_Config.m_ClVideoSndEnable))
	{
		pSound->Mix((short *)pStream, Len / sizeof(short) / 2);
	}
	else
	{
		mem_zero(pStream, Len);
	}
#else
	pSound->Mix((short *)pStream, Len / sizeof(short) / 2);
#endif
}

int CSound::Init()
{
	m_SoundEnabled = false;
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	// Initialize sample indices. We always need them to load sounds in
	// the editor even if sound is disabled or failed to be enabled.
	const CLockScope LockScope(m_SoundLock);
	m_FirstFreeSampleIndex = 0;
	for(size_t i = 0; i < std::size(m_aSamples) - 1; ++i)
	{
		m_aSamples[i].m_Index = i;
		m_aSamples[i].m_NextFreeSampleIndex = i + 1;
		m_aSamples[i].m_pData = nullptr;
	}
	m_aSamples[std::size(m_aSamples) - 1].m_Index = std::size(m_aSamples) - 1;
	m_aSamples[std::size(m_aSamples) - 1].m_NextFreeSampleIndex = SAMPLE_INDEX_FULL;

	if(!g_Config.m_SndEnable)
		return 0;

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		dbg_msg("sound", "unable to init SDL audio: %s", SDL_GetError());
		return -1;
	}

	SDL_AudioSpec Format, FormatOut;
	Format.freq = g_Config.m_SndRate;
	Format.format = AUDIO_S16;
	Format.channels = 2;
	Format.samples = g_Config.m_SndBufferSize;
	Format.callback = SdlCallback;
	Format.userdata = this;

	// Open the audio device and start playing sound!
	m_Device = SDL_OpenAudioDevice(nullptr, 0, &Format, &FormatOut, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if(m_Device == 0)
	{
		dbg_msg("sound", "unable to open audio: %s", SDL_GetError());
		return -1;
	}
	else
		dbg_msg("sound", "sound init successful using audio driver '%s'", SDL_GetCurrentAudioDriver());

	m_MixingRate = FormatOut.freq;
	m_MaxFrames = FormatOut.samples * 2;
#if defined(CONF_VIDEORECORDER)
	m_MaxFrames = maximum<uint32_t>(m_MaxFrames, 1024 * 2); // make the buffer bigger just in case
#endif
	m_pMixBuffer = (int *)calloc(m_MaxFrames * 2, sizeof(int));

	m_SoundEnabled = true;
	Update();

	SDL_PauseAudioDevice(m_Device, 0);
	return 0;
}

int CSound::Update()
{
	UpdateVolume();
	return 0;
}

void CSound::UpdateVolume()
{
	int WantedVolume = g_Config.m_SndVolume;
	if(!m_pGraphics->WindowActive() && g_Config.m_SndNonactiveMute)
		WantedVolume = 0;
	m_SoundVolume.store(WantedVolume, std::memory_order_relaxed);
}

void CSound::Shutdown()
{
	StopAll();

	// Stop sound callback before freeing sample data
	SDL_CloseAudioDevice(m_Device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	m_Device = 0;

	const CLockScope LockScope(m_SoundLock);
	for(auto &Sample : m_aSamples)
	{
		free(Sample.m_pData);
		Sample.m_pData = nullptr;
	}

	free(m_pMixBuffer);
	m_pMixBuffer = nullptr;
	m_SoundEnabled = false;
}

CSample *CSound::AllocSample()
{
	const CLockScope LockScope(m_SoundLock);
	if(m_FirstFreeSampleIndex == SAMPLE_INDEX_FULL)
		return nullptr;

	CSample *pSample = &m_aSamples[m_FirstFreeSampleIndex];
	if(pSample->m_pData != nullptr || pSample->m_NextFreeSampleIndex == SAMPLE_INDEX_USED)
	{
		char aError[128];
		str_format(aError, sizeof(aError), "Sample was not unloaded (index=%d, next=%d, duration=%f, data=%p)",
			pSample->m_Index, pSample->m_NextFreeSampleIndex, pSample->TotalTime(), pSample->m_pData);
		dbg_assert(false, aError);
	}
	m_FirstFreeSampleIndex = pSample->m_NextFreeSampleIndex;
	pSample->m_NextFreeSampleIndex = SAMPLE_INDEX_USED;
	return pSample;
}

void CSound::RateConvert(CSample &Sample) const
{
	dbg_assert(Sample.IsLoaded(), "Sample not loaded");
	// make sure that we need to convert this sound
	if(Sample.m_Rate == m_MixingRate)
		return;

	// allocate new data
	const int NumFrames = (int)((Sample.m_NumFrames / (float)Sample.m_Rate) * m_MixingRate);
	short *pNewData = (short *)calloc((size_t)NumFrames * Sample.m_Channels, sizeof(short));

	for(int i = 0; i < NumFrames; i++)
	{
		// resample TODO: this should be done better, like linear at least
		float a = i / (float)NumFrames;
		int f = (int)(a * Sample.m_NumFrames);
		if(f >= Sample.m_NumFrames)
			f = Sample.m_NumFrames - 1;

		// set new data
		if(Sample.m_Channels == 1)
			pNewData[i] = Sample.m_pData[f];
		else if(Sample.m_Channels == 2)
		{
			pNewData[i * 2] = Sample.m_pData[f * 2];
			pNewData[i * 2 + 1] = Sample.m_pData[f * 2 + 1];
		}
	}

	// free old data and apply new
	free(Sample.m_pData);
	Sample.m_pData = pNewData;
	Sample.m_NumFrames = NumFrames;
	Sample.m_Rate = m_MixingRate;
}

bool CSound::DecodeOpus(CSample &Sample, const void *pData, unsigned DataSize) const
{
	int OpusError = 0;
	OggOpusFile *pOpusFile = op_open_memory((const unsigned char *)pData, DataSize, &OpusError);
	if(pOpusFile)
	{
		const int NumChannels = op_channel_count(pOpusFile, -1);
		if(NumChannels > 2)
		{
			op_free(pOpusFile);
			dbg_msg("sound/opus", "file is not mono or stereo.");
			return false;
		}

		const int NumSamples = op_pcm_total(pOpusFile, -1); // per channel!
		if(NumSamples < 0)
		{
			op_free(pOpusFile);
			dbg_msg("sound/opus", "failed to get number of samples, error %d", NumSamples);
			return false;
		}

		short *pSampleData = (short *)calloc((size_t)NumSamples * NumChannels, sizeof(short));

		int Pos = 0;
		while(Pos < NumSamples)
		{
			const int Read = op_read(pOpusFile, pSampleData + Pos * NumChannels, (NumSamples - Pos) * NumChannels, nullptr);
			if(Read < 0)
			{
				free(pSampleData);
				op_free(pOpusFile);
				dbg_msg("sound/opus", "op_read error %d at %d", Read, Pos);
				return false;
			}
			else if(Read == 0) // EOF
				break;
			Pos += Read;
		}

		op_free(pOpusFile);

		Sample.m_pData = pSampleData;
		Sample.m_NumFrames = Pos;
		Sample.m_Rate = 48000;
		Sample.m_Channels = NumChannels;
		Sample.m_LoopStart = -1;
		Sample.m_LoopEnd = -1;
		Sample.m_PausedAt = 0;
	}
	else
	{
		dbg_msg("sound/opus", "failed to decode sample, error %d", OpusError);
		return false;
	}

	return true;
}

// TODO: Update WavPack to get rid of these global variables
static const void *s_pWVBuffer = nullptr;
static int s_WVBufferPosition = 0;
static int s_WVBufferSize = 0;

static int ReadDataOld(void *pBuffer, int Size)
{
	int ChunkSize = minimum(Size, s_WVBufferSize - s_WVBufferPosition);
	mem_copy(pBuffer, (const char *)s_pWVBuffer + s_WVBufferPosition, ChunkSize);
	s_WVBufferPosition += ChunkSize;
	return ChunkSize;
}

#if defined(CONF_WAVPACK_OPEN_FILE_INPUT_EX)
static int ReadData(void *pId, void *pBuffer, int Size)
{
	(void)pId;
	return ReadDataOld(pBuffer, Size);
}

static int ReturnFalse(void *pId)
{
	(void)pId;
	return 0;
}

static unsigned int GetPos(void *pId)
{
	(void)pId;
	return s_WVBufferPosition;
}

static unsigned int GetLength(void *pId)
{
	(void)pId;
	return s_WVBufferSize;
}

static int PushBackByte(void *pId, int Char)
{
	s_WVBufferPosition -= 1;
	return 0;
}
#endif

bool CSound::DecodeWV(CSample &Sample, const void *pData, unsigned DataSize) const
{
	char aError[100];

	dbg_assert(s_pWVBuffer == nullptr, "DecodeWV already in use");
	s_pWVBuffer = pData;
	s_WVBufferSize = DataSize;
	s_WVBufferPosition = 0;

#if defined(CONF_WAVPACK_OPEN_FILE_INPUT_EX)
	WavpackStreamReader Callback = {0};
	Callback.can_seek = ReturnFalse;
	Callback.get_length = GetLength;
	Callback.get_pos = GetPos;
	Callback.push_back_byte = PushBackByte;
	Callback.read_bytes = ReadData;
	WavpackContext *pContext = WavpackOpenFileInputEx(&Callback, (void *)1, 0, aError, 0, 0);
#else
	WavpackContext *pContext = WavpackOpenFileInput(ReadDataOld, aError);
#endif
	if(pContext)
	{
		const int NumSamples = WavpackGetNumSamples(pContext);
		const int BitsPerSample = WavpackGetBitsPerSample(pContext);
		const unsigned int SampleRate = WavpackGetSampleRate(pContext);
		const int NumChannels = WavpackGetNumChannels(pContext);

		if(NumChannels > 2)
		{
			dbg_msg("sound/wv", "file is not mono or stereo.");
			s_pWVBuffer = nullptr;
			return false;
		}

		if(BitsPerSample != 16)
		{
			dbg_msg("sound/wv", "bps is %d, not 16", BitsPerSample);
			s_pWVBuffer = nullptr;
			return false;
		}

		int *pBuffer = (int *)calloc((size_t)NumSamples * NumChannels, sizeof(int));
		if(!WavpackUnpackSamples(pContext, pBuffer, NumSamples))
		{
			free(pBuffer);
			dbg_msg("sound/wv", "WavpackUnpackSamples failed. NumSamples=%d, NumChannels=%d", NumSamples, NumChannels);
			s_pWVBuffer = nullptr;
			return false;
		}

		Sample.m_pData = (short *)calloc((size_t)NumSamples * NumChannels, sizeof(short));

		int *pSrc = pBuffer;
		short *pDst = Sample.m_pData;
		for(int i = 0; i < NumSamples * NumChannels; i++)
			*pDst++ = (short)*pSrc++;

		free(pBuffer);
#ifdef CONF_WAVPACK_CLOSE_FILE
		WavpackCloseFile(pContext);
#endif

		Sample.m_NumFrames = NumSamples;
		Sample.m_Rate = SampleRate;
		Sample.m_Channels = NumChannels;
		Sample.m_LoopStart = -1;
		Sample.m_LoopEnd = -1;
		Sample.m_PausedAt = 0;

		s_pWVBuffer = nullptr;
	}
	else
	{
		dbg_msg("sound/wv", "failed to decode sample (%s)", aError);
		s_pWVBuffer = nullptr;
		return false;
	}

	return true;
}

int CSound::LoadOpus(const char *pFilename, int StorageType)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
	{
		dbg_msg("sound/opus", "failed to allocate sample ID. filename='%s'", pFilename);
		return -1;
	}

	void *pData;
	unsigned DataSize;
	if(!m_pStorage->ReadFile(pFilename, StorageType, &pData, &DataSize))
	{
		UnloadSample(pSample->m_Index);
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	const bool DecodeSuccess = DecodeOpus(*pSample, pData, DataSize);
	free(pData);
	if(!DecodeSuccess)
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	if(g_Config.m_Debug)
		dbg_msg("sound/opus", "loaded %s", pFilename);

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadWV(const char *pFilename, int StorageType)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
	{
		dbg_msg("sound/wv", "failed to allocate sample ID. filename='%s'", pFilename);
		return -1;
	}

	void *pData;
	unsigned DataSize;
	if(!m_pStorage->ReadFile(pFilename, StorageType, &pData, &DataSize))
	{
		UnloadSample(pSample->m_Index);
		dbg_msg("sound/wv", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	const bool DecodeSuccess = DecodeWV(*pSample, pData, DataSize);
	free(pData);
	if(!DecodeSuccess)
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	if(g_Config.m_Debug)
		dbg_msg("sound/wv", "loaded %s", pFilename);

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadOpusFromMem(const void *pData, unsigned DataSize, bool ForceLoad = false)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !ForceLoad)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
		return -1;

	if(!DecodeOpus(*pSample, pData, DataSize))
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadWVFromMem(const void *pData, unsigned DataSize, bool ForceLoad = false)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !ForceLoad)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
		return -1;

	if(!DecodeWV(*pSample, pData, DataSize))
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	RateConvert(*pSample);
	return pSample->m_Index;
}

void CSound::UnloadSample(int SampleId)
{
	if(SampleId == -1)
		return;

	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");
	const CLockScope LockScope(m_SoundLock);
	CSample &Sample = m_aSamples[SampleId];

	if(Sample.IsLoaded())
	{
		// Stop voices using this sample
		for(auto &Voice : m_aVoices)
		{
			if(Voice.m_pSample == &Sample)
			{
				Voice.m_pSample = nullptr;
			}
		}

		// Free data
		free(Sample.m_pData);
		Sample.m_pData = nullptr;
	}

	// Free slot
	if(Sample.m_NextFreeSampleIndex == SAMPLE_INDEX_USED)
	{
		Sample.m_NextFreeSampleIndex = m_FirstFreeSampleIndex;
		m_FirstFreeSampleIndex = Sample.m_Index;
	}
}

float CSound::GetSampleTotalTime(int SampleId)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");

	const CLockScope LockScope(m_SoundLock);
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded");
	return m_aSamples[SampleId].TotalTime();
}

float CSound::GetSampleCurrentTime(int SampleId)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");

	const CLockScope LockScope(m_SoundLock);
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded");
	CSample *pSample = &m_aSamples[SampleId];
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			return Voice.m_Tick / (float)pSample->m_Rate;
		}
	}

	return pSample->m_PausedAt / (float)pSample->m_Rate;
}

void CSound::SetSampleCurrentTime(int SampleId, float Time)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");

	const CLockScope LockScope(m_SoundLock);
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded");
	CSample *pSample = &m_aSamples[SampleId];
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			Voice.m_Tick = pSample->m_NumFrames * Time;
			return;
		}
	}

	pSample->m_PausedAt = pSample->m_NumFrames * Time;
}

void CSound::SetChannel(int ChannelId, float Vol, float Pan)
{
	dbg_assert(ChannelId >= 0 && ChannelId < NUM_CHANNELS, "ChannelId invalid");

	const CLockScope LockScope(m_SoundLock);
	m_aChannels[ChannelId].m_Vol = (int)(Vol * 255.0f);
	m_aChannels[ChannelId].m_Pan = (int)(Pan * 255.0f); // TODO: this is only on and off right now
}

void CSound::SetListenerPosition(vec2 Position)
{
	m_ListenerPositionX.store(Position.x, std::memory_order_relaxed);
	m_ListenerPositionY.store(Position.y, std::memory_order_relaxed);
}

void CSound::SetVoiceVolume(CVoiceHandle Voice, float Volume)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	Volume = clamp(Volume, 0.0f, 1.0f);
	m_aVoices[VoiceId].m_Vol = (int)(Volume * 255.0f);
}

void CSound::SetVoiceFalloff(CVoiceHandle Voice, float Falloff)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	Falloff = clamp(Falloff, 0.0f, 1.0f);
	m_aVoices[VoiceId].m_Falloff = Falloff;
}

void CSound::SetVoicePosition(CVoiceHandle Voice, vec2 Position)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceId].m_Position = Position;
}

void CSound::SetVoiceTimeOffset(CVoiceHandle Voice, float TimeOffset)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	if(!m_aVoices[VoiceId].m_pSample)
		return;

	int Tick = 0;
	bool IsLooping = m_aVoices[VoiceId].m_Flags & ISound::FLAG_LOOP;
	uint64_t TickOffset = m_aVoices[VoiceId].m_pSample->m_Rate * TimeOffset;
	if(m_aVoices[VoiceId].m_pSample->m_NumFrames > 0 && IsLooping)
		Tick = TickOffset % m_aVoices[VoiceId].m_pSample->m_NumFrames;
	else
		Tick = clamp(TickOffset, (uint64_t)0, (uint64_t)m_aVoices[VoiceId].m_pSample->m_NumFrames);

	// at least 200msec off, else depend on buffer size
	float Threshold = maximum(0.2f * m_aVoices[VoiceId].m_pSample->m_Rate, (float)m_MaxFrames);
	if(absolute(m_aVoices[VoiceId].m_Tick - Tick) > Threshold)
	{
		// take care of looping (modulo!)
		if(!(IsLooping && (minimum(m_aVoices[VoiceId].m_Tick, Tick) + m_aVoices[VoiceId].m_pSample->m_NumFrames - maximum(m_aVoices[VoiceId].m_Tick, Tick)) <= Threshold))
		{
			m_aVoices[VoiceId].m_Tick = Tick;
		}
	}
}

void CSound::SetVoiceCircle(CVoiceHandle Voice, float Radius)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceId].m_Shape = ISound::SHAPE_CIRCLE;
	m_aVoices[VoiceId].m_Circle.m_Radius = maximum(0.0f, Radius);
}

void CSound::SetVoiceRectangle(CVoiceHandle Voice, float Width, float Height)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceId].m_Shape = ISound::SHAPE_RECTANGLE;
	m_aVoices[VoiceId].m_Rectangle.m_Width = maximum(0.0f, Width);
	m_aVoices[VoiceId].m_Rectangle.m_Height = maximum(0.0f, Height);
}

ISound::CVoiceHandle CSound::Play(int ChannelId, int SampleId, int Flags, float Volume, vec2 Position)
{
	const CLockScope LockScope(m_SoundLock);

	// search for voice
	int VoiceId = -1;
	for(int i = 0; i < NUM_VOICES; i++)
	{
		int NextId = (m_NextVoice + i) % NUM_VOICES;
		if(!m_aVoices[NextId].m_pSample)
		{
			VoiceId = NextId;
			m_NextVoice = NextId + 1;
			break;
		}
	}
	if(VoiceId == -1)
	{
		return CreateVoiceHandle(-1, -1);
	}

	// voice found, use it
	m_aVoices[VoiceId].m_pSample = &m_aSamples[SampleId];
	m_aVoices[VoiceId].m_pChannel = &m_aChannels[ChannelId];
	if(Flags & FLAG_LOOP)
	{
		m_aVoices[VoiceId].m_Tick = m_aSamples[SampleId].m_PausedAt;
	}
	else if(Flags & FLAG_PREVIEW)
	{
		m_aVoices[VoiceId].m_Tick = m_aSamples[SampleId].m_PausedAt;
		m_aSamples[SampleId].m_PausedAt = 0;
	}
	else
	{
		m_aVoices[VoiceId].m_Tick = 0;
	}
	m_aVoices[VoiceId].m_Vol = (int)(clamp(Volume, 0.0f, 1.0f) * 255.0f);
	m_aVoices[VoiceId].m_Flags = Flags;
	m_aVoices[VoiceId].m_Position = Position;
	m_aVoices[VoiceId].m_Falloff = 0.0f;
	m_aVoices[VoiceId].m_Shape = ISound::SHAPE_CIRCLE;
	m_aVoices[VoiceId].m_Circle.m_Radius = 1500;
	return CreateVoiceHandle(VoiceId, m_aVoices[VoiceId].m_Age);
}

ISound::CVoiceHandle CSound::PlayAt(int ChannelId, int SampleId, int Flags, float Volume, vec2 Position)
{
	return Play(ChannelId, SampleId, Flags | ISound::FLAG_POS, Volume, Position);
}

ISound::CVoiceHandle CSound::Play(int ChannelId, int SampleId, int Flags, float Volume)
{
	return Play(ChannelId, SampleId, Flags, Volume, vec2(0.0f, 0.0f));
}

void CSound::Pause(int SampleId)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");

	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleId];
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded");
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			Voice.m_pSample->m_PausedAt = Voice.m_Tick;
			Voice.m_pSample = nullptr;
		}
	}
}

void CSound::Stop(int SampleId)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");

	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleId];
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded");
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample == pSample)
		{
			if(Voice.m_Flags & FLAG_LOOP)
				Voice.m_pSample->m_PausedAt = Voice.m_Tick;
			else
				Voice.m_pSample->m_PausedAt = 0;
			Voice.m_pSample = nullptr;
		}
	}
}

void CSound::StopAll()
{
	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	for(auto &Voice : m_aVoices)
	{
		if(Voice.m_pSample)
		{
			if(Voice.m_Flags & FLAG_LOOP)
				Voice.m_pSample->m_PausedAt = Voice.m_Tick;
			else
				Voice.m_pSample->m_PausedAt = 0;
		}
		Voice.m_pSample = nullptr;
	}
}

void CSound::StopVoice(CVoiceHandle Voice)
{
	if(!Voice.IsValid())
		return;

	int VoiceId = Voice.Id();

	const CLockScope LockScope(m_SoundLock);
	if(m_aVoices[VoiceId].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceId].m_pSample = nullptr;
	m_aVoices[VoiceId].m_Age++;
}

bool CSound::IsPlaying(int SampleId)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid");
	const CLockScope LockScope(m_SoundLock);
	const CSample *pSample = &m_aSamples[SampleId];
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded");
	return std::any_of(std::begin(m_aVoices), std::end(m_aVoices), [pSample](const auto &Voice) { return Voice.m_pSample == pSample; });
}

void CSound::PauseAudioDevice()
{
	SDL_PauseAudioDevice(m_Device, 1);
}

void CSound::UnpauseAudioDevice()
{
	SDL_PauseAudioDevice(m_Device, 0);
}

IEngineSound *CreateEngineSound() { return new CSound; }
