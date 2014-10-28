/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/storage.h>

#include <engine/shared/config.h>

#include "SDL.h"

#include "sound.h"

extern "C" { // wavpack
	#include <engine/external/wavpack/wavpack.h>
	#include <opusfile.h>
}
#include <math.h>

enum
{
	NUM_SAMPLES = 512,
	NUM_VOICES = 64,
	NUM_CHANNELS = 16,
};

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
} ;

struct CVoice
{
	CSample *m_pSample;
	CChannel *m_pChannel;
	int m_Age; // increases when reused
	int m_Tick;
	int m_Vol; // 0 - 255
	int m_FalloffDistance; // 0 - inifintee (well int)
	int m_Flags;
	int m_X, m_Y;
} ;

static CSample m_aSamples[NUM_SAMPLES] = { {0} };
static CVoice m_aVoices[NUM_VOICES] = { {0} };
static CChannel m_aChannels[NUM_CHANNELS] = { {255, 0} };

static LOCK m_SoundLock = 0;

static int m_CenterX = 0;
static int m_CenterY = 0;

static int m_MixingRate = 48000;
static volatile int m_SoundVolume = 100;

static int m_NextVoice = 0;
static int *m_pMixBuffer = 0;	// buffer only used by the thread callback function
static unsigned m_MaxFrames = 0;

static const void *ms_pWVBuffer = 0x0;
static int ms_WVBufferPosition = 0;
static int ms_WVBufferSize = 0;

const int DefaultDistance = 1500;

// TODO: there should be a faster way todo this
static short Int2Short(int i)
{
	if(i > 0x7fff)
		return 0x7fff;
	else if(i < -0x7fff)
		return -0x7fff;
	return i;
}

static int IntAbs(int i)
{
	if(i<0)
		return -i;
	return i;
}

static void Mix(short *pFinalOut, unsigned Frames)
{
	int MasterVol;
	mem_zero(m_pMixBuffer, m_MaxFrames*2*sizeof(int));
	Frames = min(Frames, m_MaxFrames);

	// aquire lock while we are mixing
	lock_wait(m_SoundLock);

	MasterVol = m_SoundVolume;

	for(unsigned i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample)
		{
			// mix voice
			CVoice *v = &m_aVoices[i];
			int *pOut = m_pMixBuffer;

			int Step = v->m_pSample->m_Channels; // setup input sources
			short *pInL = &v->m_pSample->m_pData[v->m_Tick*Step];
			short *pInR = &v->m_pSample->m_pData[v->m_Tick*Step+1];

			unsigned End = v->m_pSample->m_NumFrames-v->m_Tick;

			int Rvol = (int)(v->m_pChannel->m_Vol*(v->m_Vol/255.0f));
			int Lvol = (int)(v->m_pChannel->m_Vol*(v->m_Vol/255.0f));

			// make sure that we don't go outside the sound data
			if(Frames < End)
				End = Frames;

			// check if we have a mono sound
			if(v->m_pSample->m_Channels == 1)
				pInR = pInL;

			// volume calculation
			if(v->m_Flags&ISound::FLAG_POS && v->m_pChannel->m_Pan)
			{
				// TODO: we should respect the channel panning value
				int dx = v->m_X - m_CenterX;
				int dy = v->m_Y - m_CenterY;
				int Dist = (int)sqrtf((float)dx*dx+dy*dy); // float here. nasty
				int p = IntAbs(dx);
				int Range = v->m_FalloffDistance;
				if(Dist >= 0 && Dist < Range)
				{
					// panning
					if(dx > 0)
						Lvol = ((Range-p)*Lvol)/Range;
					else
						Rvol = ((Range-p)*Rvol)/Range;

					// falloff
					Lvol = (Lvol*(Range-Dist))/Range;
					Rvol = (Rvol*(Range-Dist))/Range;
				}
				else
				{
					Lvol = 0;
					Rvol = 0;
				}
			}

			// process all frames
			for(unsigned s = 0; s < End; s++)
			{
				*pOut++ += (*pInL)*Lvol;
				*pOut++ += (*pInR)*Rvol;
				pInL += Step;
				pInR += Step;
				v->m_Tick++;
			}

			// free voice if not used any more
			if(v->m_Tick == v->m_pSample->m_NumFrames)
			{
				if(v->m_Flags&ISound::FLAG_LOOP)
					v->m_Tick = 0;
				else
				{
					v->m_pSample = 0;
					v->m_Age++;
				}
			}
		}
	}


	// release the lock
	lock_release(m_SoundLock);

	{
		// clamp accumulated values
		// TODO: this seams slow
		for(unsigned i = 0; i < Frames; i++)
		{
			int j = i<<1;
			int vl = ((m_pMixBuffer[j]*MasterVol)/101)>>8;
			int vr = ((m_pMixBuffer[j+1]*MasterVol)/101)>>8;

			pFinalOut[j] = Int2Short(vl);
			pFinalOut[j+1] = Int2Short(vr);
		}
	}

#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(pFinalOut, sizeof(short), Frames * 2);
#endif
}

static void SdlCallback(void *pUnused, Uint8 *pStream, int Len)
{
	(void)pUnused;
	Mix((short *)pStream, Len/2/2);
}


int CSound::Init()
{
	m_SoundEnabled = 0;
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	SDL_AudioSpec Format, FormatOut;

	m_SoundLock = lock_create();

	if(!g_Config.m_SndEnable)
		return 0;

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		dbg_msg("gfx", "unable to init SDL audio: %s", SDL_GetError());
		return -1;
	}

	m_MixingRate = g_Config.m_SndRate;

	// Set 16-bit stereo audio at 22Khz
	Format.freq = g_Config.m_SndRate; // ignore_convention
	Format.format = AUDIO_S16; // ignore_convention
	Format.channels = 2; // ignore_convention
	Format.samples = g_Config.m_SndBufferSize; // ignore_convention
	Format.callback = SdlCallback; // ignore_convention
	Format.userdata = NULL; // ignore_convention

	// Open the audio device and start playing sound!
	if(SDL_OpenAudio(&Format, &FormatOut) < 0)
	{
		dbg_msg("client/sound", "unable to open audio: %s", SDL_GetError());
		return -1;
	}
	else
		dbg_msg("client/sound", "sound init successful");

	m_MaxFrames = FormatOut.samples*2;
	m_pMixBuffer = (int *)mem_alloc(m_MaxFrames*2*sizeof(int), 1);

	SDL_PauseAudio(0);

	m_SoundEnabled = 1;
	Update(); // update the volume
	return 0;
}

int CSound::Update()
{
	// update volume
	int WantedVolume = g_Config.m_SndVolume;

	if(!m_pGraphics->WindowActive() && g_Config.m_SndNonactiveMute)
		WantedVolume = 0;

	if(WantedVolume != m_SoundVolume)
	{
		lock_wait(m_SoundLock);
		m_SoundVolume = WantedVolume;
		lock_release(m_SoundLock);
	}

	return 0;
}

int CSound::Shutdown()
{
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	lock_destroy(m_SoundLock);
	if(m_pMixBuffer)
	{
		mem_free(m_pMixBuffer);
		m_pMixBuffer = 0;
	}
	return 0;
}

int CSound::AllocID()
{
	// TODO: linear search, get rid of it
	for(unsigned SampleID = 0; SampleID < NUM_SAMPLES; SampleID++)
	{
		if(m_aSamples[SampleID].m_pData == 0x0)
			return SampleID;
	}

	return -1;
}

void CSound::RateConvert(int SampleID)
{
	CSample *pSample = &m_aSamples[SampleID];
	int NumFrames = 0;
	short *pNewData = 0;

	// make sure that we need to convert this sound
	if(!pSample->m_pData || pSample->m_Rate == m_MixingRate)
		return;

	// allocate new data
	NumFrames = (int)((pSample->m_NumFrames/(float)pSample->m_Rate)*m_MixingRate);
	pNewData = (short *)mem_alloc(NumFrames*pSample->m_Channels*sizeof(short), 1);

	for(int i = 0; i < NumFrames; i++)
	{
		// resample TODO: this should be done better, like linear atleast
		float a = i/(float)NumFrames;
		int f = (int)(a*pSample->m_NumFrames);
		if(f >= pSample->m_NumFrames)
			f = pSample->m_NumFrames-1;

		// set new data
		if(pSample->m_Channels == 1)
			pNewData[i] = pSample->m_pData[f];
		else if(pSample->m_Channels == 2)
		{
			pNewData[i*2] = pSample->m_pData[f*2];
			pNewData[i*2+1] = pSample->m_pData[f*2+1];
		}
	}

	// free old data and apply new
	mem_free(pSample->m_pData);
	pSample->m_pData = pNewData;
	pSample->m_NumFrames = NumFrames;
	pSample->m_Rate = m_MixingRate;
}

int CSound::ReadData(void *pBuffer, int Size)
{
	int ChunkSize = min(Size, ms_WVBufferSize - ms_WVBufferPosition);
	mem_copy(pBuffer, (const char *)ms_pWVBuffer + ms_WVBufferPosition, ChunkSize);
	ms_WVBufferPosition += ChunkSize;
	return ChunkSize;
}

int CSound::DecodeOpus(int SampleID, const void *pData, unsigned DataSize)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return -1;

	CSample *pSample = &m_aSamples[SampleID];
	char aError[100];

	ms_pWVBuffer = pData;
	ms_WVBufferSize = DataSize;
	ms_WVBufferPosition = 0;

	OggOpusFile *OpusFile = op_open_memory((const unsigned char *) pData, DataSize, NULL);
	if (OpusFile)
	{
		int NumChannels = op_channel_count(OpusFile, -1);
		int NumSamples = op_pcm_total(OpusFile, -1); // per channel!

		pSample->m_Channels = NumChannels;

		if(pSample->m_Channels > 2)
		{
			dbg_msg("sound/opus", "file is not mono or stereo.");
			return -1;
		}

		pSample->m_pData = (short *)mem_alloc(NumSamples * sizeof(short) * NumChannels, 1);

		int Read;
		int Pos = 0;
		while (Pos < NumSamples)
		{
			Read = op_read(OpusFile, pSample->m_pData + Pos*NumChannels, NumSamples*NumChannels, NULL);
			Pos += Read;
		}

		pSample->m_NumFrames = NumSamples; // ?
		pSample->m_Rate = 48000;
		pSample->m_LoopStart = -1;
		pSample->m_LoopEnd = -1;
		pSample->m_PausedAt = 0;
	}
	else
	{
		dbg_msg("sound/opus", "failed to decode sample (%s)", aError);
	}

	return SampleID;
}

int CSound::DecodeWV(int SampleID, const void *pData, unsigned DataSize)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return -1;

	CSample *pSample = &m_aSamples[SampleID];
	char aError[100];
	WavpackContext *pContext;

	ms_pWVBuffer = pData;
	ms_WVBufferSize = DataSize;
	ms_WVBufferPosition = 0;

	pContext = WavpackOpenFileInput(ReadData, aError);
	if (pContext)
	{
		int NumSamples = WavpackGetNumSamples(pContext);
		int BitsPerSample = WavpackGetBitsPerSample(pContext);
		unsigned int SampleRate = WavpackGetSampleRate(pContext);
		int NumChannels = WavpackGetNumChannels(pContext);
		int *pSrc;
		short *pDst;
		int i;

		pSample->m_Channels = NumChannels;
		pSample->m_Rate = SampleRate;

		if(pSample->m_Channels > 2)
		{
			dbg_msg("sound/wv", "file is not mono or stereo.");
			return -1;
		}

		if(BitsPerSample != 16)
		{
			dbg_msg("sound/wv", "bps is %d, not 16", BitsPerSample);
			return -1;
		}

		int *pBuffer = (int *)mem_alloc(4*NumSamples*NumChannels, 1);
		WavpackUnpackSamples(pContext, pBuffer, NumSamples); // TODO: check return value
		pSrc = pBuffer;

		pSample->m_pData = (short *)mem_alloc(2*NumSamples*NumChannels, 1);
		pDst = pSample->m_pData;

		for (i = 0; i < NumSamples*NumChannels; i++)
			*pDst++ = (short)*pSrc++;

		mem_free(pBuffer);

		pSample->m_NumFrames = NumSamples;
		pSample->m_LoopStart = -1;
		pSample->m_LoopEnd = -1;
		pSample->m_PausedAt = 0;
	}
	else
	{
		dbg_msg("sound/wv", "failed to decode sample (%s)", aError);
	}

	return SampleID;
}

int CSound::LoadOpus(const char *pFilename)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	if(!m_pStorage)
		return -1;

	ms_File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!ms_File)
	{
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	// read the whole file into memory
	int DataSize = io_length(ms_File);

	if(DataSize <= 0)
	{
		io_close(ms_File);
		dbg_msg("sound/opus", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	char *pData = new char[DataSize];
	io_read(ms_File, pData, DataSize);

	SampleID = DecodeOpus(SampleID, pData, DataSize);

	delete[] pData;
	io_close(ms_File);
	ms_File = NULL;

	if(g_Config.m_Debug)
		dbg_msg("sound/opus", "loaded %s", pFilename);

	RateConvert(SampleID);
	return SampleID;
}


int CSound::LoadWV(const char *pFilename)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return -1;

	if(!m_pStorage)
		return -1;

	ms_File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!ms_File)
	{
		dbg_msg("sound/wv", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	// read the whole file into memory
	int DataSize = io_length(ms_File);

	if(DataSize <= 0)
	{
		io_close(ms_File);
		dbg_msg("sound/wv", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	char *pData = new char[DataSize];
	io_read(ms_File, pData, DataSize);

	SampleID = DecodeWV(SampleID, pData, DataSize);

	delete[] pData;
	io_close(ms_File);
	ms_File = NULL;

	if(g_Config.m_Debug)
		dbg_msg("sound/wv", "loaded %s", pFilename);

	RateConvert(SampleID);
	return SampleID;
}

int CSound::LoadOpusFromMem(const void *pData, unsigned DataSize, bool FromEditor = false)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !FromEditor)
		return -1;

	if(!pData)
		return -1;

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	SampleID = DecodeOpus(SampleID, pData, DataSize);

	RateConvert(SampleID);
	return SampleID;
}

int CSound::LoadWVFromMem(const void *pData, unsigned DataSize, bool FromEditor = false)
{
	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled && !FromEditor)
		return -1;

	if(!pData)
		return -1;

	int SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	SampleID = DecodeWV(SampleID, pData, DataSize);

	RateConvert(SampleID);
	return SampleID;
}

void CSound::UnloadSample(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return;

	Stop(SampleID);
	mem_free(m_aSamples[SampleID].m_pData);

	m_aSamples[SampleID].m_pData = 0x0;
}

float CSound::GetSampleDuration(int SampleID)
{
	if(SampleID == -1 || SampleID >= NUM_SAMPLES)
		return 0.0f;

	return (m_aSamples[SampleID].m_NumFrames/m_aSamples[SampleID].m_Rate);
}

void CSound::SetListenerPos(float x, float y)
{
	m_CenterX = (int)x;
	m_CenterY = (int)y;
}

void CSound::SetVoiceVolume(CVoiceHandle Voice, float Volume)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	Volume = clamp(Volume, 0.0f, 1.0f);
	m_aVoices[VoiceID].m_Vol = (int)(Volume*255.0f);
}

void CSound::SetVoiceMaxDistance(CVoiceHandle Voice, int Distance)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	if(Distance < 0)
		return;

	m_aVoices[VoiceID].m_FalloffDistance = Distance;
}

void CSound::SetVoiceLocation(CVoiceHandle Voice, float x, float y)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	m_aVoices[VoiceID].m_X = x;
	m_aVoices[VoiceID].m_Y = y;
}

void CSound::SetVoiceTimeOffset(CVoiceHandle Voice, float offset)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	lock_wait(m_SoundLock);
	{
		if(m_aVoices[VoiceID].m_pSample)
		{
			int TickOffset = m_aVoices[VoiceID].m_pSample->m_Rate * offset;
			if(m_aVoices[VoiceID].m_pSample->m_NumFrames > 0 && m_aVoices[VoiceID].m_Flags&ISound::FLAG_POS)
				TickOffset = TickOffset % m_aVoices[VoiceID].m_pSample->m_NumFrames;
			else
				TickOffset = clamp(TickOffset, 0, m_aVoices[VoiceID].m_pSample->m_NumFrames);

			// at least 2sec off
			if(abs(m_aVoices[VoiceID].m_Tick-TickOffset) > 2*m_aVoices[VoiceID].m_pSample->m_Rate)
				m_aVoices[VoiceID].m_Tick = TickOffset;
		}
	}
	lock_release(m_SoundLock);
}

void CSound::SetChannel(int ChannelID, float Vol, float Pan)
{
	m_aChannels[ChannelID].m_Vol = (int)(Vol*255.0f);
	m_aChannels[ChannelID].m_Pan = (int)(Pan*255.0f); // TODO: this is only on and off right now
}

ISound::CVoiceHandle CSound::Play(int ChannelID, int SampleID, int Flags, float x, float y)
{
	int VoiceID = -1;
	int Age = -1;
	int i;

	lock_wait(m_SoundLock);

	// search for voice
	for(i = 0; i < NUM_VOICES; i++)
	{
		int id = (m_NextVoice + i) % NUM_VOICES;
		if(!m_aVoices[id].m_pSample)
		{
			VoiceID = id;
			m_NextVoice = id+1;
			break;
		}
	}

	// voice found, use it
	if(VoiceID != -1)
	{
		m_aVoices[VoiceID].m_pSample = &m_aSamples[SampleID];
		m_aVoices[VoiceID].m_pChannel = &m_aChannels[ChannelID];
		if(Flags & FLAG_LOOP)
			m_aVoices[VoiceID].m_Tick = m_aSamples[SampleID].m_PausedAt;
		else
			m_aVoices[VoiceID].m_Tick = 0;
		m_aVoices[VoiceID].m_Vol = 255;
		m_aVoices[VoiceID].m_Flags = Flags;
		m_aVoices[VoiceID].m_X = (int)x;
		m_aVoices[VoiceID].m_Y = (int)y;
		m_aVoices[VoiceID].m_FalloffDistance = DefaultDistance;
		Age = m_aVoices[VoiceID].m_Age;
	}

	lock_release(m_SoundLock);
	return CreateVoiceHandle(VoiceID, Age);
}

ISound::CVoiceHandle CSound::PlayAt(int ChannelID, int SampleID, int Flags, float x, float y)
{
	return Play(ChannelID, SampleID, Flags|ISound::FLAG_POS, x, y);
}

ISound::CVoiceHandle CSound::Play(int ChannelID, int SampleID, int Flags)
{
	return Play(ChannelID, SampleID, Flags, 0, 0);
}

void CSound::Stop(int SampleID)
{
	// TODO: a nice fade out
	lock_wait(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(int i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample == pSample)
		{
			if(m_aVoices[i].m_Flags & FLAG_LOOP)
				m_aVoices[i].m_pSample->m_PausedAt = m_aVoices[i].m_Tick;
			else
				m_aVoices[i].m_pSample->m_PausedAt = 0;
			m_aVoices[i].m_pSample = 0;
		}
	}
	lock_release(m_SoundLock);
}

void CSound::StopAll()
{
	// TODO: a nice fade out
	lock_wait(m_SoundLock);
	for(int i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample)
		{
			if(m_aVoices[i].m_Flags & FLAG_LOOP)
				m_aVoices[i].m_pSample->m_PausedAt = m_aVoices[i].m_Tick;
			else
				m_aVoices[i].m_pSample->m_PausedAt = 0;
		}
		m_aVoices[i].m_pSample = 0;
	}
	lock_release(m_SoundLock);
}

void CSound::StopVoice(CVoiceHandle Voice)
{
	if(!Voice.IsValid())
		return;

	int VoiceID = Voice.Id();

	if(m_aVoices[VoiceID].m_Age != Voice.Age())
		return;

	lock_wait(m_SoundLock);
	{
		m_aVoices[VoiceID].m_pSample = 0;
		m_aVoices[VoiceID].m_Age++;
	}
	lock_release(m_SoundLock);
}


IOHANDLE CSound::ms_File = 0;

IEngineSound *CreateEngineSound() { return new CSound; }

