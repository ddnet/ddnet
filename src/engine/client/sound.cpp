/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "sound.h"

#include <base/bytes.h>
#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <SDL.h>

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

unsigned CSound::AdvanceVoice(CVoice &Voice, unsigned Frames)
{
	// make sure that we don't go outside the sound data
	const unsigned Advanced = std::min(Frames, (unsigned)(Voice.m_pSample->m_NumFrames - Voice.m_Tick));
	Voice.m_Tick += Advanced;

	// free voice if not used any more
	if(Voice.m_Tick == Voice.m_pSample->m_NumFrames)
	{
		if(Voice.m_Flags & ISound::FLAG_LOOP)
		{
			Voice.m_Tick = Voice.m_pSample->m_LoopStart;
		}
		else
		{
			Voice.m_pSample = nullptr;
			Voice.m_Age++;
		}
	}

	return Advanced;
}

void CSound::Mix(short *pFinalOut, unsigned Frames)
{
	Frames = std::min(Frames, m_MaxFrames);
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

		const CSample *pSample = Voice.m_pSample;
		const int Step = pSample->m_Channels; // setup input sources
		short *pInL = &pSample->m_pData[Voice.m_Tick * Step];
		short *pInR = &pSample->m_pData[Voice.m_Tick * Step + 1];

		const unsigned End = AdvanceVoice(Voice, Frames);

		int VolumeR = round_truncate(Voice.m_pChannel->m_Vol * (Voice.m_Vol / 255.0f));
		int VolumeL = VolumeR;

		// check if we have a mono sound
		if(pSample->m_Channels == 1)
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
		}
	}

	m_SoundLock.unlock();

	// clamp accumulated values
	for(unsigned i = 0; i < Frames * 2; i++)
		pFinalOut[i] = std::clamp<int>(((m_pMixBuffer[i] * MasterVol) / 101) >> 8, std::numeric_limits<short>::min(), std::numeric_limits<short>::max());

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
		log_error("sound", "Unable to init SDL audio: %s", SDL_GetError());
		return -1;
	}

	m_AudioSpec.freq = g_Config.m_SndRate;
	m_AudioSpec.format = AUDIO_S16;
	m_AudioSpec.channels = 2;
	m_AudioSpec.samples = g_Config.m_SndBufferSize;
	m_AudioSpec.callback = SdlCallback;
	m_AudioSpec.userdata = this;

	m_MixingRate = m_AudioSpec.freq;
	m_MaxFrames = m_AudioSpec.samples * 2;
#if defined(CONF_VIDEORECORDER)
	m_MaxFrames = std::max(m_MaxFrames, 1024u * 2u); // make the buffer bigger just in case
#endif
	m_pMixBuffer = (int *)calloc(m_MaxFrames * 2, sizeof(int));

	m_SoundEnabled = true;
	UpdateVolume();

	SDL_AddEventWatch(HandleAudioDeviceEvent, this);
	if(OpenDevice(true))
	{
		log_info("sound", "Sound init successful using audio driver '%s'", SDL_GetCurrentAudioDriver());
	}
	else
	{
		log_error("sound", "Unable to open audio device (%s), waiting for one to become available", SDL_GetError());
	}
	return 0;
}

int SDLCALL CSound::HandleAudioDeviceEvent(void *pUser, SDL_Event *pEvent)
{
	if((pEvent->type == SDL_AUDIODEVICEADDED || pEvent->type == SDL_AUDIODEVICEREMOVED) && !pEvent->adevice.iscapture)
	{
		static_cast<CSound *>(pUser)->m_DeviceChanged.store(true, std::memory_order_relaxed);
	}
	return 0;
}

bool CSound::OpenDevice(bool AllowFrequencyChange)
{
	dbg_assert(m_Device == 0, "Audio device already open");

	SDL_AudioSpec FormatOut;
	m_Device = SDL_OpenAudioDevice(nullptr, 0, &m_AudioSpec, &FormatOut, AllowFrequencyChange ? SDL_AUDIO_ALLOW_FREQUENCY_CHANGE : 0);
	if(m_Device == 0)
		return false;

	if(AllowFrequencyChange)
	{
		// Samples are converted to this rate when they are loaded, so later devices are asked for it
		m_MixingRate = FormatOut.freq;
		m_AudioSpec.freq = m_MixingRate;
	}
	SDL_PauseAudioDevice(m_Device, m_DevicePaused ? 1 : 0);
	return true;
}

void CSound::CloseDevice()
{
	if(m_Device == 0)
		return;

	SDL_CloseAudioDevice(m_Device);
	m_Device = 0;
}

void CSound::UpdateDevice()
{
	if(!m_SoundEnabled)
		return;

	if(m_Device != 0)
	{
		if(SDL_GetAudioDeviceStatus(m_Device) != SDL_AUDIO_STOPPED)
			return;
		log_info("sound", "Audio device was disconnected");
		CloseDevice();
	}

	// Opening the device when the system has none blocks for up to eight seconds
	// in SDL's WASAPI backend, which would stall the main loop
	if(SDL_GetNumAudioDevices(0) <= 0)
		return;

	if(!m_DeviceChanged.exchange(false, std::memory_order_relaxed))
		return;

	if(OpenDevice(false))
	{
		log_info("sound", "Audio device connected, using audio driver '%s'", SDL_GetCurrentAudioDriver());
	}
}

bool CSound::HasAudioOutput() const
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && g_Config.m_ClVideoSndEnable)
		return true;
#endif
	return m_Device != 0;
}

int CSound::Update()
{
	UpdateVolume();
	UpdateDevice();
	AdvancePlayback();
	return 0;
}

void CSound::AdvancePlayback()
{
	if(!m_SoundEnabled || HasAudioOutput())
	{
		m_PlaybackTime = 0;
		return;
	}

	// Advance the voices in real time while there is no device, so that sounds
	// end and loop like they would during playback
	const int64_t Now = time_get();
	if(m_PlaybackTime == 0)
	{
		m_PlaybackTime = Now;
		return;
	}

	// Drop the backlog after the client was blocked for a long time
	m_PlaybackTime = std::max(m_PlaybackTime, Now - time_freq());

	int64_t Frames = ((Now - m_PlaybackTime) * m_MixingRate) / time_freq();
	if(Frames <= 0)
		return;
	// Only count the whole frames, keeping the remainder for the next update
	m_PlaybackTime += (Frames * time_freq()) / m_MixingRate;

	const CLockScope LockScope(m_SoundLock);
	while(Frames > 0)
	{
		// Advance in the same chunks as the mixer, so voices loop at the same points
		const unsigned ChunkFrames = std::min<unsigned>(Frames, m_MaxFrames);
		for(auto &Voice : m_aVoices)
		{
			if(Voice.m_pSample)
				AdvanceVoice(Voice, ChunkFrames);
		}
		Frames -= ChunkFrames;
	}
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
	SDL_DelEventWatch(HandleAudioDeviceEvent, this);
	CloseDevice();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

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
	dbg_assert(
		pSample->m_pData == nullptr && pSample->m_NextFreeSampleIndex != SAMPLE_INDEX_USED,
		"Sample was not unloaded (index=%d, next=%d, duration=%f, data=%p)",
		pSample->m_Index, pSample->m_NextFreeSampleIndex, pSample->TotalTime(), pSample->m_pData);
	m_FirstFreeSampleIndex = pSample->m_NextFreeSampleIndex;
	pSample->m_NextFreeSampleIndex = SAMPLE_INDEX_USED;
	return pSample;
}

void CSound::RateConvert(CSample &Sample) const
{
	dbg_assert(Sample.IsLoaded(), "Sample not loaded: %d", Sample.m_Index);
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

	// adjust looping position, note that this is not precise
	const double Factor = (double)m_MixingRate / (double)Sample.m_Rate;
	Sample.m_LoopStart = std::round(Sample.m_LoopStart * Factor);

	// free old data and apply new
	free(Sample.m_pData);
	Sample.m_pData = pNewData;
	Sample.m_NumFrames = NumFrames;
	Sample.m_Rate = m_MixingRate;
}

bool CSound::DecodeOpus(CSample &Sample, const void *pData, unsigned DataSize, const char *pContextName) const
{
	int OpusError = 0;
	OggOpusFile *pOpusFile = op_open_memory((const unsigned char *)pData, DataSize, &OpusError);
	if(pOpusFile)
	{
		const int NumChannels = op_channel_count(pOpusFile, -1);
		if(NumChannels > 2)
		{
			op_free(pOpusFile);
			log_error("sound/opus", "File is not mono or stereo. Filename='%s'", pContextName);
			return false;
		}

		const int NumSamples = op_pcm_total(pOpusFile, -1); // per channel!
		if(NumSamples < 0)
		{
			op_free(pOpusFile);
			log_error("sound/opus", "Failed to get number of samples, error %d. Filename='%s'", NumSamples, pContextName);
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
				log_error("sound/opus", "op_read error %d at %d. Filename='%s'", Read, Pos, pContextName);
				return false;
			}
			else if(Read == 0) // EOF
				break;
			Pos += Read;
		}

		Sample.m_pData = pSampleData;
		Sample.m_NumFrames = Pos;
		Sample.m_Rate = 48000;
		Sample.m_Channels = NumChannels;
		Sample.m_LoopStart = 0;
		Sample.m_PausedAt = 0;

		const OpusTags *pTags = op_tags(pOpusFile, -1);
		if(pTags)
		{
			for(int i = 0; i < pTags->comments; ++i)
			{
				const char *pComment = pTags->user_comments[i];
				if(!pComment)
					continue;
				if(!str_startswith(pComment, "LOOP_START="))
					continue;
				int LoopStart = -1;
				if(!str_toint(pComment + str_length("LOOP_START="), &LoopStart))
				{
					log_error("sound/opus", "Invalid LOOP_START tag. Value='%s' Filename='%s'", pComment + str_length("LOOP_START="), pContextName);
					break;
				}
				if(LoopStart < 0 || LoopStart >= Sample.m_NumFrames)
				{
					log_error("sound/opus", "Tag LOOP_START out of range. Value=%d Min=0 Max=%d Filename='%s'", LoopStart, Sample.m_NumFrames - 1, pContextName);
					break;
				}
				Sample.m_LoopStart = LoopStart;
				break;
			}
		}

		op_free(pOpusFile);
	}
	else
	{
		log_error("sound/opus", "Failed to decode sample, error %d. Filename='%s'", OpusError, pContextName);
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
	int ChunkSize = std::min(Size, s_WVBufferSize - s_WVBufferPosition);
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

bool CSound::DecodeWV(CSample &Sample, const void *pData, unsigned DataSize, const char *pContextName) const
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return false;

	dbg_assert(s_pWVBuffer == nullptr, "DecodeWV already in use");
	s_pWVBuffer = pData;
	s_WVBufferSize = DataSize;
	s_WVBufferPosition = 0;

	char aError[100];

#if defined(CONF_WAVPACK_OPEN_FILE_INPUT_EX)
	WavpackStreamReader Callback = {};
	Callback.can_seek = ReturnFalse;
	Callback.get_length = GetLength;
	Callback.get_pos = GetPos;
	Callback.push_back_byte = PushBackByte;
	Callback.read_bytes = ReadData;
	WavpackContext *pContext = WavpackOpenFileInputEx(&Callback, (void *)1, nullptr, aError, 0, 0);
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
			log_error("sound/wv", "File is not mono or stereo. Filename='%s'", pContextName);
			s_pWVBuffer = nullptr;
			return false;
		}

		if(BitsPerSample != 16)
		{
			log_error("sound/wv", "Bits per sample is %d, not 16. Filename='%s'", BitsPerSample, pContextName);
			s_pWVBuffer = nullptr;
			return false;
		}

		int *pBuffer = (int *)calloc((size_t)NumSamples * NumChannels, sizeof(int));
		if(!WavpackUnpackSamples(pContext, pBuffer, NumSamples))
		{
			free(pBuffer);
			log_error("sound/wv", "WavpackUnpackSamples failed. NumSamples=%d NumChannels=%d Filename='%s'", NumSamples, NumChannels, pContextName);
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
		Sample.m_LoopStart = 0;
		Sample.m_PausedAt = 0;

		s_pWVBuffer = nullptr;
	}
	else
	{
		log_error("sound/wv", "Failed to decode sample (%s). Filename='%s'", aError, pContextName);
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
		log_error("sound/opus", "Failed to allocate sample ID. Filename='%s'", pFilename);
		return -1;
	}

	void *pData;
	unsigned DataSize;
	if(!m_pStorage->ReadFile(pFilename, StorageType, &pData, &DataSize))
	{
		UnloadSample(pSample->m_Index);
		log_error("sound/opus", "Failed to open file. Filename='%s'", pFilename);
		return -1;
	}

	const bool DecodeSuccess = DecodeOpus(*pSample, pData, DataSize, pFilename);
	free(pData);
	if(!DecodeSuccess)
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	if(g_Config.m_Debug)
		log_trace("sound/opus", "Loaded '%s' (index %d)", pFilename, pSample->m_Index);

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
		log_error("sound/wv", "Failed to allocate sample ID. Filename='%s'", pFilename);
		return -1;
	}

	void *pData;
	unsigned DataSize;
	if(!m_pStorage->ReadFile(pFilename, StorageType, &pData, &DataSize))
	{
		UnloadSample(pSample->m_Index);
		log_error("sound/wv", "Failed to open file. Filename='%s'", pFilename);
		return -1;
	}

	const bool DecodeSuccess = DecodeWV(*pSample, pData, DataSize, pFilename);
	free(pData);
	if(!DecodeSuccess)
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	if(g_Config.m_Debug)
		log_trace("sound/wv", "Loaded '%s' (index %d)", pFilename, pSample->m_Index);

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadOpusFromMem(const void *pData, unsigned DataSize, bool ForceLoad, const char *pContextName)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !ForceLoad)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
		return -1;

	if(!DecodeOpus(*pSample, pData, DataSize, pContextName))
	{
		UnloadSample(pSample->m_Index);
		return -1;
	}

	RateConvert(*pSample);
	return pSample->m_Index;
}

int CSound::LoadWVFromMem(const void *pData, unsigned DataSize, bool ForceLoad, const char *pContextName)
{
	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !ForceLoad)
		return -1;

	CSample *pSample = AllocSample();
	if(!pSample)
		return -1;

	if(!DecodeWV(*pSample, pData, DataSize, pContextName))
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

	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);
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
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);

	const CLockScope LockScope(m_SoundLock);
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded: %d", SampleId);
	return m_aSamples[SampleId].TotalTime();
}

float CSound::GetSampleCurrentTime(int SampleId)
{
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);

	const CLockScope LockScope(m_SoundLock);
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded: %d", SampleId);
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
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);

	const CLockScope LockScope(m_SoundLock);
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded: %d", SampleId);
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
	dbg_assert(ChannelId >= 0 && ChannelId < NUM_CHANNELS, "ChannelId invalid: %d", ChannelId);

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

	Volume = std::clamp(Volume, 0.0f, 1.0f);
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

	Falloff = std::clamp(Falloff, 0.0f, 1.0f);
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
	{
		const int LoopStart = m_aVoices[VoiceId].m_pSample->m_LoopStart;
		const int NumFrames = m_aVoices[VoiceId].m_pSample->m_NumFrames;
		if(TickOffset < static_cast<uint64_t>(NumFrames))
		{
			// Still in first playthrough
			Tick = TickOffset;
		}
		else
		{
			// Past first playthrough, wrap within loop section only
			const int LoopLength = NumFrames - LoopStart;
			if(LoopLength > 0)
				Tick = LoopStart + ((TickOffset - NumFrames) % LoopLength);
			else
				Tick = LoopStart;
		}
	}
	else
	{
		Tick = std::clamp<uint64_t>(TickOffset, 0, m_aVoices[VoiceId].m_pSample->m_NumFrames);
	}

	// at least 200msec off, else depend on buffer size
	float Threshold = std::max(0.2f * m_aVoices[VoiceId].m_pSample->m_Rate, (float)m_MaxFrames);
	if(absolute(m_aVoices[VoiceId].m_Tick - Tick) > Threshold)
	{
		// take care of looping (modulo!)
		if(!(IsLooping && (std::min(m_aVoices[VoiceId].m_Tick, Tick) + m_aVoices[VoiceId].m_pSample->m_NumFrames - std::max(m_aVoices[VoiceId].m_Tick, Tick)) <= Threshold))
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
	m_aVoices[VoiceId].m_Circle.m_Radius = std::max(0.0f, Radius);
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
	m_aVoices[VoiceId].m_Rectangle.m_Width = std::max(0.0f, Width);
	m_aVoices[VoiceId].m_Rectangle.m_Height = std::max(0.0f, Height);
}

ISound::CVoiceHandle CSound::Play(int ChannelId, int SampleId, int Flags, float Volume, vec2 Position)
{
	dbg_assert(ChannelId >= 0 && ChannelId < NUM_CHANNELS, "ChannelId invalid: %d", ChannelId);
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);

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
	m_aVoices[VoiceId].m_Vol = (int)(std::clamp(Volume, 0.0f, 1.0f) * 255.0f);
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
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);

	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleId];
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded: %d", SampleId);
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
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);

	// TODO: a nice fade out
	const CLockScope LockScope(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleId];
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded: %d", SampleId);
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
	dbg_assert(SampleId >= 0 && SampleId < NUM_SAMPLES, "SampleId invalid: %d", SampleId);
	const CLockScope LockScope(m_SoundLock);
	const CSample *pSample = &m_aSamples[SampleId];
	dbg_assert(m_aSamples[SampleId].IsLoaded(), "Sample not loaded: %d", SampleId);
	return std::any_of(std::begin(m_aVoices), std::end(m_aVoices), [pSample](const auto &Voice) { return Voice.m_pSample == pSample; });
}

void CSound::PauseAudioDevice()
{
	m_DevicePaused = true;
	if(m_Device != 0)
	{
		SDL_PauseAudioDevice(m_Device, 1);
	}
}

void CSound::UnpauseAudioDevice()
{
	m_DevicePaused = false;
	if(m_Device != 0)
	{
		SDL_PauseAudioDevice(m_Device, 0);
	}
}

IEngineSound *CreateEngineSound() { return new CSound; }
