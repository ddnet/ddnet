#if defined(CONF_VIDEORECORDER)

#include "video.h"

#include <base/log.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/sound.h>
#include <engine/storage.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
};

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

// This code is mostly stolen from https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/muxing.c

#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */

#if LIBAVCODEC_VERSION_MAJOR >= 60
#define FRAME_NUM frame_num
#else
#define FRAME_NUM frame_number
#endif

const size_t FORMAT_GL_NCHANNELS = 4;
CLock g_WriteLock;

static LEVEL AvLevelToLogLevel(int Level)
{
	switch(Level)
	{
	case AV_LOG_PANIC:
	case AV_LOG_FATAL:
	case AV_LOG_ERROR:
		return LEVEL_ERROR;
	case AV_LOG_WARNING:
		return LEVEL_WARN;
	case AV_LOG_INFO:
		return LEVEL_INFO;
	case AV_LOG_VERBOSE:
	case AV_LOG_DEBUG:
		return LEVEL_DEBUG;
	case AV_LOG_TRACE:
		return LEVEL_TRACE;
	default:
		dbg_assert(false, "invalid log level");
		dbg_break();
	}
}

void AvLogCallback(void *pUser, int Level, const char *pFormat, va_list VarArgs)
	GNUC_ATTRIBUTE((format(printf, 3, 0)));

void AvLogCallback(void *pUser, int Level, const char *pFormat, va_list VarArgs)
{
	const LEVEL LogLevel = AvLevelToLogLevel(Level);
	if(LogLevel <= LEVEL_INFO)
	{
		log_log_v(LogLevel, "videorecorder/libav", pFormat, VarArgs);
	}
}

void CVideo::Init()
{
	av_log_set_callback(AvLogCallback);
}

CVideo::CVideo(IGraphics *pGraphics, ISound *pSound, IStorage *pStorage, int Width, int Height, const char *pName) :
	m_pGraphics(pGraphics),
	m_pStorage(pStorage),
	m_pSound(pSound)
{
	m_pFormatContext = nullptr;
	m_pFormat = nullptr;
	m_pOptDict = nullptr;

	m_pVideoCodec = nullptr;
	m_pAudioCodec = nullptr;

	m_Width = Width;
	m_Height = Height;
	str_copy(m_aName, pName);

	m_FPS = g_Config.m_ClVideoRecorderFPS;

	m_Recording = false;
	m_Started = false;
	m_Stopped = false;
	m_ProcessingVideoFrame = 0;
	m_ProcessingAudioFrame = 0;

	m_HasAudio = m_pSound->IsSoundEnabled() && g_Config.m_ClVideoSndEnable;

	dbg_assert(ms_pCurrentVideo == nullptr, "ms_pCurrentVideo is NOT set to nullptr while creating a new Video.");

	ms_TickTime = time_freq() / m_FPS;
	ms_pCurrentVideo = this;
}

CVideo::~CVideo()
{
	ms_pCurrentVideo = nullptr;
}

bool CVideo::Start()
{
	dbg_assert(!m_Started, "Already started");

	// wait for the graphic thread to idle
	m_pGraphics->WaitForIdle();

	m_AudioStream = {};
	m_VideoStream = {};

	char aWholePath[IO_MAX_PATH_LENGTH];
	IOHANDLE File = m_pStorage->OpenFile(m_aName, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath));
	if(File)
	{
		io_close(File);
	}
	else
	{
		log_error("videorecorder", "Could not open file '%s'", aWholePath);
		return false;
	}

	const int FormatAllocResult = avformat_alloc_output_context2(&m_pFormatContext, nullptr, "mp4", aWholePath);
	if(FormatAllocResult < 0 || !m_pFormatContext)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(FormatAllocResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not create format context: %s", aError);
		return false;
	}

	m_pFormat = m_pFormatContext->oformat;

#if defined(CONF_ARCH_IA32) || defined(CONF_ARCH_ARM)
	// use only the minimum of 2 threads on 32-bit to save memory
	m_VideoThreads = 2;
	m_AudioThreads = 2;
#else
	m_VideoThreads = std::thread::hardware_concurrency() + 2;
	// audio gets a bit less
	m_AudioThreads = (std::thread::hardware_concurrency() / 2) + 2;
#endif

	m_CurVideoThreadIndex = 0;
	m_CurAudioThreadIndex = 0;

	size_t GLNVals = FORMAT_GL_NCHANNELS * m_Width * m_Height;
	m_vVideoBuffers.resize(m_VideoThreads);
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		m_vVideoBuffers[i].m_vBuffer.resize(GLNVals * sizeof(uint8_t));
	}

	m_vAudioBuffers.resize(m_AudioThreads);

	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if(m_pFormat->video_codec != AV_CODEC_ID_NONE)
	{
		if(!AddStream(&m_VideoStream, m_pFormatContext, &m_pVideoCodec, m_pFormat->video_codec))
			return false;
	}
	else
	{
		log_error("videorecorder", "Could not determine default video stream codec");
		return false;
	}

	if(m_HasAudio)
	{
		if(m_pFormat->audio_codec != AV_CODEC_ID_NONE)
		{
			if(!AddStream(&m_AudioStream, m_pFormatContext, &m_pAudioCodec, m_pFormat->audio_codec))
				return false;
		}
		else
		{
			log_error("videorecorder", "Could not determine default audio stream codec");
			return false;
		}
	}

	m_vpVideoThreads.resize(m_VideoThreads);
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		m_vpVideoThreads[i] = std::make_unique<CVideoRecorderThread>();
	}
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		std::unique_lock<std::mutex> Lock(m_vpVideoThreads[i]->m_Mutex);
		m_vpVideoThreads[i]->m_Thread = std::thread([this, i]() REQUIRES(!g_WriteLock) { RunVideoThread(i == 0 ? (m_VideoThreads - 1) : (i - 1), i); });
		m_vpVideoThreads[i]->m_Cond.wait(Lock, [this, i]() -> bool { return m_vpVideoThreads[i]->m_Started; });
	}

	m_vpAudioThreads.resize(m_AudioThreads);
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		m_vpAudioThreads[i] = std::make_unique<CAudioRecorderThread>();
	}
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		std::unique_lock<std::mutex> Lock(m_vpAudioThreads[i]->m_Mutex);
		m_vpAudioThreads[i]->m_Thread = std::thread([this, i]() REQUIRES(!g_WriteLock) { RunAudioThread(i == 0 ? (m_AudioThreads - 1) : (i - 1), i); });
		m_vpAudioThreads[i]->m_Cond.wait(Lock, [this, i]() -> bool { return m_vpAudioThreads[i]->m_Started; });
	}

	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if(!OpenVideo())
		return false;

	if(m_HasAudio && !OpenAudio())
		return false;

	/* open the output file, if needed */
	if(!(m_pFormat->flags & AVFMT_NOFILE))
	{
		const int OpenResult = avio_open(&m_pFormatContext->pb, aWholePath, AVIO_FLAG_WRITE);
		if(OpenResult < 0)
		{
			char aError[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(OpenResult, aError, sizeof(aError));
			log_error("videorecorder", "Could not open file '%s': %s", aWholePath, aError);
			return false;
		}
	}

	m_VideoStream.m_vpSwsContexts.reserve(m_VideoThreads);

	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		if(m_VideoStream.m_vpSwsContexts.size() <= i)
			m_VideoStream.m_vpSwsContexts.emplace_back(nullptr);

		if(!m_VideoStream.m_vpSwsContexts[i])
		{
			m_VideoStream.m_vpSwsContexts[i] = sws_getCachedContext(
				m_VideoStream.m_vpSwsContexts[i],
				m_VideoStream.m_pCodecContext->width, m_VideoStream.m_pCodecContext->height, AV_PIX_FMT_RGBA,
				m_VideoStream.m_pCodecContext->width, m_VideoStream.m_pCodecContext->height, AV_PIX_FMT_YUV420P,
				0, 0, 0, 0);
		}
	}

	/* Write the stream header, if any. */
	const int WriteHeaderResult = avformat_write_header(m_pFormatContext, &m_pOptDict);
	if(WriteHeaderResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(WriteHeaderResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not write header: %s", aError);
		return false;
	}

	m_Recording = true;
	m_Started = true;
	m_Stopped = false;
	ms_Time = time_get();
	return true;
}

void CVideo::Pause(bool Pause)
{
	if(ms_pCurrentVideo)
		m_Recording = !Pause;
}

void CVideo::Stop()
{
	dbg_assert(!m_Stopped, "Already stopped");
	m_Stopped = true;

	m_pGraphics->WaitForIdle();

	for(auto &pVideoThread : m_vpVideoThreads)
	{
		{
			std::unique_lock<std::mutex> Lock(pVideoThread->m_Mutex);
			pVideoThread->m_Finished = true;
			pVideoThread->m_Cond.notify_all();
		}

		pVideoThread->m_Thread.join();
	}
	m_vpVideoThreads.clear();

	for(auto &pAudioThread : m_vpAudioThreads)
	{
		{
			std::unique_lock<std::mutex> Lock(pAudioThread->m_Mutex);
			pAudioThread->m_Finished = true;
			pAudioThread->m_Cond.notify_all();
		}

		pAudioThread->m_Thread.join();
	}
	m_vpAudioThreads.clear();

	while(m_ProcessingVideoFrame > 0 || m_ProcessingAudioFrame > 0)
		std::this_thread::sleep_for(10us);

	m_Recording = false;

	FinishFrames(&m_VideoStream);

	if(m_HasAudio)
		FinishFrames(&m_AudioStream);

	if(m_pFormatContext && m_Started)
		av_write_trailer(m_pFormatContext);

	CloseStream(&m_VideoStream);

	if(m_HasAudio)
		CloseStream(&m_AudioStream);

	if(m_pFormatContext)
	{
		if(!(m_pFormat->flags & AVFMT_NOFILE))
			avio_closep(&m_pFormatContext->pb);

		avformat_free_context(m_pFormatContext);
	}

	ISound *volatile pSound = m_pSound;

	pSound->PauseAudioDevice();
	delete ms_pCurrentVideo;
	pSound->UnpauseAudioDevice();
}

void CVideo::NextVideoFrameThread()
{
	if(m_Recording)
	{
		m_VideoFrameIndex += 1;
		if(m_VideoFrameIndex >= 2)
		{
			m_ProcessingVideoFrame.fetch_add(1);

			size_t NextVideoThreadIndex = m_CurVideoThreadIndex + 1;
			if(NextVideoThreadIndex == m_VideoThreads)
				NextVideoThreadIndex = 0;

			// always wait for the next video thread too, to prevent a dead lock
			{
				auto *pVideoThread = m_vpVideoThreads[NextVideoThreadIndex].get();
				std::unique_lock<std::mutex> Lock(pVideoThread->m_Mutex);

				if(pVideoThread->m_HasVideoFrame)
				{
					pVideoThread->m_Cond.wait(Lock, [&pVideoThread]() -> bool { return !pVideoThread->m_HasVideoFrame; });
				}
			}

			// after reading the graphic libraries' frame buffer, go threaded
			{
				auto *pVideoThread = m_vpVideoThreads[m_CurVideoThreadIndex].get();
				std::unique_lock<std::mutex> Lock(pVideoThread->m_Mutex);

				if(pVideoThread->m_HasVideoFrame)
				{
					pVideoThread->m_Cond.wait(Lock, [&pVideoThread]() -> bool { return !pVideoThread->m_HasVideoFrame; });
				}

				UpdateVideoBufferFromGraphics(m_CurVideoThreadIndex);

				pVideoThread->m_HasVideoFrame = true;
				{
					std::unique_lock<std::mutex> LockParent(pVideoThread->m_VideoFillMutex);
					pVideoThread->m_VideoFrameToFill = m_VideoFrameIndex;
				}
				pVideoThread->m_Cond.notify_all();
			}

			++m_CurVideoThreadIndex;
			if(m_CurVideoThreadIndex == m_VideoThreads)
				m_CurVideoThreadIndex = 0;
		}
	}
}

void CVideo::NextVideoFrame()
{
	if(m_Recording)
	{
		ms_Time += ms_TickTime;
		ms_LocalTime = (ms_Time - ms_LocalStartTime) / (float)time_freq();
	}
}

void CVideo::NextAudioFrameTimeline(ISoundMixFunc Mix)
{
	if(m_Recording && m_HasAudio)
	{
		double SamplesPerFrame = (double)m_AudioStream.m_pCodecContext->sample_rate / m_FPS;
		while(m_AudioStream.m_SamplesFrameCount >= m_AudioStream.m_SamplesCount)
		{
			NextAudioFrame(Mix);
		}
		m_AudioStream.m_SamplesFrameCount += SamplesPerFrame;
	}
}

void CVideo::NextAudioFrame(ISoundMixFunc Mix)
{
	if(m_Recording && m_HasAudio)
	{
		m_AudioFrameIndex += 1;

		m_ProcessingAudioFrame.fetch_add(1);

		size_t NextAudioThreadIndex = m_CurAudioThreadIndex + 1;
		if(NextAudioThreadIndex == m_AudioThreads)
			NextAudioThreadIndex = 0;

		// always wait for the next Audio thread too, to prevent a dead lock

		{
			auto *pAudioThread = m_vpAudioThreads[NextAudioThreadIndex].get();
			std::unique_lock<std::mutex> Lock(pAudioThread->m_Mutex);

			if(pAudioThread->m_HasAudioFrame)
			{
				pAudioThread->m_Cond.wait(Lock, [&pAudioThread]() -> bool { return !pAudioThread->m_HasAudioFrame; });
			}
		}

		// after reading the graphic libraries' frame buffer, go threaded
		{
			auto *pAudioThread = m_vpAudioThreads[m_CurAudioThreadIndex].get();

			std::unique_lock<std::mutex> Lock(pAudioThread->m_Mutex);

			if(pAudioThread->m_HasAudioFrame)
			{
				pAudioThread->m_Cond.wait(Lock, [&pAudioThread]() -> bool { return !pAudioThread->m_HasAudioFrame; });
			}

			Mix(m_vAudioBuffers[m_CurAudioThreadIndex].m_aBuffer, std::size(m_vAudioBuffers[m_CurAudioThreadIndex].m_aBuffer) / 2 / 2); // two channels

			int64_t DstNbSamples = av_rescale_rnd(
				swr_get_delay(m_AudioStream.m_vpSwrContexts[m_CurAudioThreadIndex], m_AudioStream.m_pCodecContext->sample_rate) +
					m_AudioStream.m_vpFrames[m_CurAudioThreadIndex]->nb_samples,
				m_AudioStream.m_pCodecContext->sample_rate,
				m_AudioStream.m_pCodecContext->sample_rate, AV_ROUND_UP);

			pAudioThread->m_SampleCountStart = m_AudioStream.m_SamplesCount;
			m_AudioStream.m_SamplesCount += DstNbSamples;

			pAudioThread->m_HasAudioFrame = true;
			{
				std::unique_lock<std::mutex> LockParent(pAudioThread->m_AudioFillMutex);
				pAudioThread->m_AudioFrameToFill = m_AudioFrameIndex;
			}
			pAudioThread->m_Cond.notify_all();
		}

		++m_CurAudioThreadIndex;
		if(m_CurAudioThreadIndex == m_AudioThreads)
			m_CurAudioThreadIndex = 0;
	}
}

void CVideo::RunAudioThread(size_t ParentThreadIndex, size_t ThreadIndex)
{
	auto *pThreadData = m_vpAudioThreads[ThreadIndex].get();
	auto *pParentThreadData = m_vpAudioThreads[ParentThreadIndex].get();
	std::unique_lock<std::mutex> Lock(pThreadData->m_Mutex);
	pThreadData->m_Started = true;
	pThreadData->m_Cond.notify_all();

	while(!pThreadData->m_Finished)
	{
		pThreadData->m_Cond.wait(Lock, [&pThreadData]() -> bool { return pThreadData->m_HasAudioFrame || pThreadData->m_Finished; });
		pThreadData->m_Cond.notify_all();

		if(pThreadData->m_HasAudioFrame)
		{
			FillAudioFrame(ThreadIndex);
			// check if we need to wait for the parent to finish
			{
				std::unique_lock<std::mutex> LockParent(pParentThreadData->m_AudioFillMutex);
				if(pParentThreadData->m_AudioFrameToFill != 0 && pThreadData->m_AudioFrameToFill >= pParentThreadData->m_AudioFrameToFill)
				{
					// wait for the parent to finish its frame
					pParentThreadData->m_AudioFillCond.wait(LockParent, [&pParentThreadData]() -> bool { return pParentThreadData->m_AudioFrameToFill == 0; });
				}
			}
			{
				std::unique_lock<std::mutex> LockAudio(pThreadData->m_AudioFillMutex);

				{
					CLockScope ls(g_WriteLock);
					m_AudioStream.m_vpFrames[ThreadIndex]->pts = av_rescale_q(pThreadData->m_SampleCountStart, AVRational{1, m_AudioStream.m_pCodecContext->sample_rate}, m_AudioStream.m_pCodecContext->time_base);
					WriteFrame(&m_AudioStream, ThreadIndex);
				}

				pThreadData->m_AudioFrameToFill = 0;
				pThreadData->m_AudioFillCond.notify_all();
				pThreadData->m_Cond.notify_all();
			}
			m_ProcessingAudioFrame.fetch_sub(1);

			pThreadData->m_HasAudioFrame = false;
		}
	}
}

void CVideo::FillAudioFrame(size_t ThreadIndex)
{
	const int FillArrayResult = av_samples_fill_arrays(
		(uint8_t **)m_AudioStream.m_vpTmpFrames[ThreadIndex]->data,
		nullptr, // pointer to linesize (int*)
		(const uint8_t *)m_vAudioBuffers[ThreadIndex].m_aBuffer,
		2, // channels
		m_AudioStream.m_vpTmpFrames[ThreadIndex]->nb_samples,
		AV_SAMPLE_FMT_S16,
		0 // align
	);
	if(FillArrayResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(FillArrayResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not fill audio frame: %s", aError);
		return;
	}

	const int MakeWriteableResult = av_frame_make_writable(m_AudioStream.m_vpFrames[ThreadIndex]);
	if(MakeWriteableResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(MakeWriteableResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not make audio frame writeable: %s", aError);
		return;
	}

	/* convert to destination format */
	const int ConvertResult = swr_convert(
		m_AudioStream.m_vpSwrContexts[ThreadIndex],
		m_AudioStream.m_vpFrames[ThreadIndex]->data,
		m_AudioStream.m_vpFrames[ThreadIndex]->nb_samples,
		(const uint8_t **)m_AudioStream.m_vpTmpFrames[ThreadIndex]->data,
		m_AudioStream.m_vpTmpFrames[ThreadIndex]->nb_samples);
	if(ConvertResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ConvertResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not convert audio frame: %s", aError);
		return;
	}
}

void CVideo::RunVideoThread(size_t ParentThreadIndex, size_t ThreadIndex)
{
	auto *pThreadData = m_vpVideoThreads[ThreadIndex].get();
	auto *pParentThreadData = m_vpVideoThreads[ParentThreadIndex].get();
	std::unique_lock<std::mutex> Lock(pThreadData->m_Mutex);
	pThreadData->m_Started = true;
	pThreadData->m_Cond.notify_all();

	while(!pThreadData->m_Finished)
	{
		pThreadData->m_Cond.wait(Lock, [&pThreadData]() -> bool { return pThreadData->m_HasVideoFrame || pThreadData->m_Finished; });
		pThreadData->m_Cond.notify_all();

		if(pThreadData->m_HasVideoFrame)
		{
			FillVideoFrame(ThreadIndex);
			// check if we need to wait for the parent to finish
			{
				std::unique_lock<std::mutex> LockParent(pParentThreadData->m_VideoFillMutex);
				if(pParentThreadData->m_VideoFrameToFill != 0 && pThreadData->m_VideoFrameToFill >= pParentThreadData->m_VideoFrameToFill)
				{
					// wait for the parent to finish its frame
					pParentThreadData->m_VideoFillCond.wait(LockParent, [&pParentThreadData]() -> bool { return pParentThreadData->m_VideoFrameToFill == 0; });
				}
			}
			{
				std::unique_lock<std::mutex> LockVideo(pThreadData->m_VideoFillMutex);
				{
					CLockScope ls(g_WriteLock);
					m_VideoStream.m_vpFrames[ThreadIndex]->pts = (int64_t)m_VideoStream.m_pCodecContext->FRAME_NUM;
					WriteFrame(&m_VideoStream, ThreadIndex);
				}

				pThreadData->m_VideoFrameToFill = 0;
				pThreadData->m_VideoFillCond.notify_all();
				pThreadData->m_Cond.notify_all();
			}
			m_ProcessingVideoFrame.fetch_sub(1);

			pThreadData->m_HasVideoFrame = false;
		}
	}
}

void CVideo::FillVideoFrame(size_t ThreadIndex)
{
	const int InLineSize = 4 * m_VideoStream.m_pCodecContext->width;
	auto *pRGBAData = m_vVideoBuffers[ThreadIndex].m_vBuffer.data();
	sws_scale(m_VideoStream.m_vpSwsContexts[ThreadIndex], (const uint8_t *const *)&pRGBAData, &InLineSize, 0,
		m_VideoStream.m_pCodecContext->height, m_VideoStream.m_vpFrames[ThreadIndex]->data, m_VideoStream.m_vpFrames[ThreadIndex]->linesize);
}

void CVideo::UpdateVideoBufferFromGraphics(size_t ThreadIndex)
{
	uint32_t Width;
	uint32_t Height;
	CImageInfo::EImageFormat Format;
	m_pGraphics->GetReadPresentedImageDataFuncUnsafe()(Width, Height, Format, m_vVideoBuffers[ThreadIndex].m_vBuffer);
	dbg_assert((int)Width == m_Width && (int)Height == m_Height, "Size mismatch between video and graphics");
	dbg_assert(Format == CImageInfo::FORMAT_RGBA, "Unexpected image format");
}

AVFrame *CVideo::AllocPicture(enum AVPixelFormat PixFmt, int Width, int Height)
{
	AVFrame *pPicture = av_frame_alloc();
	if(!pPicture)
	{
		log_error("videorecorder", "Could not allocate video frame");
		return nullptr;
	}

	pPicture->format = PixFmt;
	pPicture->width = Width;
	pPicture->height = Height;

	/* allocate the buffers for the frame data */
	const int FrameBufferAllocResult = av_frame_get_buffer(pPicture, 32);
	if(FrameBufferAllocResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(FrameBufferAllocResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not allocate video frame buffer: %s", aError);
		return nullptr;
	}

	return pPicture;
}

AVFrame *CVideo::AllocAudioFrame(enum AVSampleFormat SampleFmt, uint64_t ChannelLayout, int SampleRate, int NbSamples)
{
	AVFrame *pFrame = av_frame_alloc();
	if(!pFrame)
	{
		log_error("videorecorder", "Could not allocate audio frame");
		return nullptr;
	}

	pFrame->format = SampleFmt;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
	dbg_assert(av_channel_layout_from_mask(&pFrame->ch_layout, ChannelLayout) == 0, "Failed to set channel layout");
#else
	pFrame->channel_layout = ChannelLayout;
#endif
	pFrame->sample_rate = SampleRate;
	pFrame->nb_samples = NbSamples;

	if(NbSamples)
	{
		const int FrameBufferAllocResult = av_frame_get_buffer(pFrame, 0);
		if(FrameBufferAllocResult < 0)
		{
			char aError[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(FrameBufferAllocResult, aError, sizeof(aError));
			log_error("videorecorder", "Could not allocate audio frame buffer: %s", aError);
			return nullptr;
		}
	}

	return pFrame;
}

bool CVideo::OpenVideo()
{
	AVCodecContext *pContext = m_VideoStream.m_pCodecContext;
	AVDictionary *pOptions = nullptr;
	av_dict_copy(&pOptions, m_pOptDict, 0);

	/* open the codec */
	const int VideoOpenResult = avcodec_open2(pContext, m_pVideoCodec, &pOptions);
	av_dict_free(&pOptions);
	if(VideoOpenResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(VideoOpenResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not open video codec: %s", aError);
		return false;
	}

	m_VideoStream.m_vpFrames.clear();
	m_VideoStream.m_vpFrames.reserve(m_VideoThreads);

	/* allocate and init a re-usable frame */
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		m_VideoStream.m_vpFrames.emplace_back(nullptr);
		m_VideoStream.m_vpFrames[i] = AllocPicture(pContext->pix_fmt, pContext->width, pContext->height);
		if(!m_VideoStream.m_vpFrames[i])
		{
			return false;
		}
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	m_VideoStream.m_vpTmpFrames.clear();
	m_VideoStream.m_vpTmpFrames.reserve(m_VideoThreads);

	if(pContext->pix_fmt != AV_PIX_FMT_YUV420P)
	{
		/* allocate and init a re-usable frame */
		for(size_t i = 0; i < m_VideoThreads; ++i)
		{
			m_VideoStream.m_vpTmpFrames.emplace_back(nullptr);
			m_VideoStream.m_vpTmpFrames[i] = AllocPicture(AV_PIX_FMT_YUV420P, pContext->width, pContext->height);
			if(!m_VideoStream.m_vpTmpFrames[i])
			{
				return false;
			}
		}
	}

	/* copy the stream parameters to the muxer */
	const int AudioStreamCopyResult = avcodec_parameters_from_context(m_VideoStream.m_pStream->codecpar, pContext);
	if(AudioStreamCopyResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(AudioStreamCopyResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not copy video stream parameters: %s", aError);
		return false;
	}
	m_VideoFrameIndex = 0;
	return true;
}

bool CVideo::OpenAudio()
{
	AVCodecContext *pContext = m_AudioStream.m_pCodecContext;
	AVDictionary *pOptions = nullptr;
	av_dict_copy(&pOptions, m_pOptDict, 0);

	/* open it */
	const int AudioOpenResult = avcodec_open2(pContext, m_pAudioCodec, &pOptions);
	av_dict_free(&pOptions);
	if(AudioOpenResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(AudioOpenResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not open audio codec: %s", aError);
		return false;
	}

	int NbSamples;
	if(pContext->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		NbSamples = 10000;
	else
		NbSamples = pContext->frame_size;

	m_AudioStream.m_vpFrames.clear();
	m_AudioStream.m_vpFrames.reserve(m_AudioThreads);

	m_AudioStream.m_vpTmpFrames.clear();
	m_AudioStream.m_vpTmpFrames.reserve(m_AudioThreads);

	/* allocate and init a re-usable frame */
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		m_AudioStream.m_vpFrames.emplace_back(nullptr);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
		m_AudioStream.m_vpFrames[i] = AllocAudioFrame(pContext->sample_fmt, pContext->ch_layout.u.mask, pContext->sample_rate, NbSamples);
#else
		m_AudioStream.m_vpFrames[i] = AllocAudioFrame(pContext->sample_fmt, pContext->channel_layout, pContext->sample_rate, NbSamples);
#endif
		if(!m_AudioStream.m_vpFrames[i])
		{
			return false;
		}

		m_AudioStream.m_vpTmpFrames.emplace_back(nullptr);
		m_AudioStream.m_vpTmpFrames[i] = AllocAudioFrame(AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_STEREO, m_pSound->MixingRate(), NbSamples);
		if(!m_AudioStream.m_vpTmpFrames[i])
		{
			return false;
		}
	}

	/* copy the stream parameters to the muxer */
	const int AudioStreamCopyResult = avcodec_parameters_from_context(m_AudioStream.m_pStream->codecpar, pContext);
	if(AudioStreamCopyResult < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(AudioStreamCopyResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not copy audio stream parameters: %s", aError);
		return false;
	}

	/* create resampling context */
	m_AudioStream.m_vpSwrContexts.clear();
	m_AudioStream.m_vpSwrContexts.resize(m_AudioThreads);
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		m_AudioStream.m_vpSwrContexts[i] = swr_alloc();
		if(!m_AudioStream.m_vpSwrContexts[i])
		{
			log_error("videorecorder", "Could not allocate resampling context");
			return false;
		}

		/* set options */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
		dbg_assert(av_opt_set_chlayout(m_AudioStream.m_vpSwrContexts[i], "in_chlayout", &pContext->ch_layout, 0) == 0, "invalid option");
#else
		dbg_assert(av_opt_set_int(m_AudioStream.m_vpSwrContexts[i], "in_channel_count", pContext->channels, 0) == 0, "invalid option");
#endif
		if(av_opt_set_int(m_AudioStream.m_vpSwrContexts[i], "in_sample_rate", m_pSound->MixingRate(), 0) != 0)
		{
			log_error("videorecorder", "Could not set audio sample rate to %d", m_pSound->MixingRate());
			return false;
		}
		dbg_assert(av_opt_set_sample_fmt(m_AudioStream.m_vpSwrContexts[i], "in_sample_fmt", AV_SAMPLE_FMT_S16, 0) == 0, "invalid option");
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
		dbg_assert(av_opt_set_chlayout(m_AudioStream.m_vpSwrContexts[i], "out_chlayout", &pContext->ch_layout, 0) == 0, "invalid option");
#else
		dbg_assert(av_opt_set_int(m_AudioStream.m_vpSwrContexts[i], "out_channel_count", pContext->channels, 0) == 0, "invalid option");
#endif
		dbg_assert(av_opt_set_int(m_AudioStream.m_vpSwrContexts[i], "out_sample_rate", pContext->sample_rate, 0) == 0, "invalid option");
		dbg_assert(av_opt_set_sample_fmt(m_AudioStream.m_vpSwrContexts[i], "out_sample_fmt", pContext->sample_fmt, 0) == 0, "invalid option");

		/* initialize the resampling context */
		const int ResamplingContextInitResult = swr_init(m_AudioStream.m_vpSwrContexts[i]);
		if(ResamplingContextInitResult < 0)
		{
			char aError[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ResamplingContextInitResult, aError, sizeof(aError));
			log_error("videorecorder", "Could not initialize resampling context: %s", aError);
			return false;
		}
	}

	m_AudioFrameIndex = 0;
	return true;
}

/* Add an output stream. */
bool CVideo::AddStream(COutputStream *pStream, AVFormatContext *pFormatContext, const AVCodec **ppCodec, enum AVCodecID CodecId) const
{
	/* find the encoder */
	*ppCodec = avcodec_find_encoder(CodecId);
	if(!(*ppCodec))
	{
		log_error("videorecorder", "Could not find encoder for codec '%s'", avcodec_get_name(CodecId));
		return false;
	}

	pStream->m_pStream = avformat_new_stream(pFormatContext, nullptr);
	if(!pStream->m_pStream)
	{
		log_error("videorecorder", "Could not allocate stream");
		return false;
	}
	pStream->m_pStream->id = pFormatContext->nb_streams - 1;
	AVCodecContext *pContext = avcodec_alloc_context3(*ppCodec);
	if(!pContext)
	{
		log_error("videorecorder", "Could not allocate encoding context");
		return false;
	}
	pStream->m_pCodecContext = pContext;

#if defined(CONF_ARCH_IA32) || defined(CONF_ARCH_ARM)
	// use only 1 ffmpeg thread on 32-bit to save memory
	pContext->thread_count = 1;
#endif

	switch((*ppCodec)->type)
	{
	case AVMEDIA_TYPE_AUDIO:
	{
		const AVSampleFormat *pSampleFormats = nullptr;
		const int *pSampleRates = nullptr;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
		avcodec_get_supported_config(pContext, *ppCodec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **)&pSampleFormats, nullptr);
		avcodec_get_supported_config(pContext, *ppCodec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void **)&pSampleRates, nullptr);
#else
		pSampleFormats = (*ppCodec)->sample_fmts;
		pSampleRates = (*ppCodec)->supported_samplerates;
#endif
		pContext->sample_fmt = pSampleFormats ? pSampleFormats[0] : AV_SAMPLE_FMT_FLTP;
		if(pSampleRates)
		{
			pContext->sample_rate = pSampleRates[0];
			for(int i = 0; pSampleRates[i]; i++)
			{
				if(pSampleRates[i] == m_pSound->MixingRate())
				{
					pContext->sample_rate = m_pSound->MixingRate();
					break;
				}
			}
		}
		else
		{
			pContext->sample_rate = m_pSound->MixingRate();
		}
		pContext->bit_rate = pContext->sample_rate * 2 * 16;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
		dbg_assert(av_channel_layout_from_mask(&pContext->ch_layout, AV_CH_LAYOUT_STEREO) == 0, "Failed to set channel layout");
#else
		pContext->channels = 2;
		pContext->channel_layout = AV_CH_LAYOUT_STEREO;
#endif

		pStream->m_pStream->time_base.num = 1;
		pStream->m_pStream->time_base.den = pContext->sample_rate;
		break;
	}

	case AVMEDIA_TYPE_VIDEO:
		pContext->codec_id = CodecId;

		pContext->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		pContext->width = m_Width;
		pContext->height = m_Height % 2 == 0 ? m_Height : m_Height - 1;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		pStream->m_pStream->time_base.num = 1;
		pStream->m_pStream->time_base.den = m_FPS;
		pContext->time_base = pStream->m_pStream->time_base;

		pContext->gop_size = 12; /* emit one intra frame every twelve frames at most */
		pContext->pix_fmt = STREAM_PIX_FMT;
		if(pContext->codec_id == AV_CODEC_ID_MPEG2VIDEO)
		{
			/* just for testing, we also add B-frames */
			pContext->max_b_frames = 2;
		}
		if(pContext->codec_id == AV_CODEC_ID_MPEG1VIDEO)
		{
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			pContext->mb_decision = 2;
		}
		if(CodecId == AV_CODEC_ID_H264)
		{
			static const char *s_apPresets[10] = {"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow", "placebo"};
			dbg_assert(g_Config.m_ClVideoX264Preset < (int)std::size(s_apPresets), "preset index invalid");
			dbg_assert(av_opt_set(pContext->priv_data, "preset", s_apPresets[g_Config.m_ClVideoX264Preset], 0) == 0, "invalid option");
			dbg_assert(av_opt_set_int(pContext->priv_data, "crf", g_Config.m_ClVideoX264Crf, 0) == 0, "invalid option");
		}
		break;

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if(pFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
		pContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return true;
}

void CVideo::WriteFrame(COutputStream *pStream, size_t ThreadIndex)
{
	AVPacket *pPacket = av_packet_alloc();
	if(pPacket == nullptr)
	{
		log_error("videorecorder", "Could not allocate packet");
		return;
	}

	pPacket->data = 0;
	pPacket->size = 0;

	avcodec_send_frame(pStream->m_pCodecContext, pStream->m_vpFrames[ThreadIndex]);
	int RecvResult = 0;
	do
	{
		RecvResult = avcodec_receive_packet(pStream->m_pCodecContext, pPacket);
		if(!RecvResult)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(pPacket, pStream->m_pCodecContext->time_base, pStream->m_pStream->time_base);
			pPacket->stream_index = pStream->m_pStream->index;

			const int WriteFrameResult = av_interleaved_write_frame(m_pFormatContext, pPacket);
			if(WriteFrameResult < 0)
			{
				char aError[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(WriteFrameResult, aError, sizeof(aError));
				log_error("videorecorder", "Could not write video frame: %s", aError);
			}
		}
		else
			break;
	} while(true);

	if(RecvResult && RecvResult != AVERROR(EAGAIN))
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(RecvResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not encode video frame: %s", aError);
	}

	av_packet_free(&pPacket);
}

void CVideo::FinishFrames(COutputStream *pStream)
{
	if(!pStream->m_pCodecContext || !avcodec_is_open(pStream->m_pCodecContext))
		return;

	AVPacket *pPacket = av_packet_alloc();
	if(pPacket == nullptr)
	{
		log_error("videorecorder", "Could not allocate packet");
		return;
	}

	pPacket->data = 0;
	pPacket->size = 0;

	avcodec_send_frame(pStream->m_pCodecContext, 0);
	int RecvResult = 0;
	do
	{
		RecvResult = avcodec_receive_packet(pStream->m_pCodecContext, pPacket);
		if(!RecvResult)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(pPacket, pStream->m_pCodecContext->time_base, pStream->m_pStream->time_base);
			pPacket->stream_index = pStream->m_pStream->index;

			const int WriteFrameResult = av_interleaved_write_frame(m_pFormatContext, pPacket);
			if(WriteFrameResult < 0)
			{
				char aError[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(WriteFrameResult, aError, sizeof(aError));
				log_error("videorecorder", "Could not write video frame: %s", aError);
			}
		}
		else
			break;
	} while(true);

	if(RecvResult && RecvResult != AVERROR_EOF)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(RecvResult, aError, sizeof(aError));
		log_error("videorecorder", "Could not finish recording: %s", aError);
	}

	av_packet_free(&pPacket);
}

void CVideo::CloseStream(COutputStream *pStream)
{
	avcodec_free_context(&pStream->m_pCodecContext);

	for(auto *pFrame : pStream->m_vpFrames)
		av_frame_free(&pFrame);
	pStream->m_vpFrames.clear();

	for(auto *pFrame : pStream->m_vpTmpFrames)
		av_frame_free(&pFrame);
	pStream->m_vpTmpFrames.clear();

	for(auto *pSwsContext : pStream->m_vpSwsContexts)
		sws_freeContext(pSwsContext);
	pStream->m_vpSwsContexts.clear();

	for(auto *pSwrContext : pStream->m_vpSwrContexts)
		swr_free(&pSwrContext);
	pStream->m_vpSwrContexts.clear();
}

#endif
