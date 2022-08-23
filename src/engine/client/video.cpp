#if defined(CONF_VIDEORECORDER)

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <base/lock_scope.h>
#include <engine/client/graphics_threaded.h>
#include <engine/sound.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
};

#include <memory>
#include <mutex>

#include "video.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// This code is mostly stolen from https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/muxing.c

#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */

const size_t FORMAT_GL_NCHANNELS = 4;
LOCK g_WriteLock = 0;

CVideo::CVideo(CGraphics_Threaded *pGraphics, ISound *pSound, IStorage *pStorage, int Width, int Height, const char *pName) :
	m_pGraphics(pGraphics),
	m_pStorage(pStorage),
	m_pSound(pSound)
{
	m_pFormatContext = 0;
	m_pFormat = 0;
	m_pOptDict = 0;

	m_pVideoCodec = 0;
	m_pAudioCodec = 0;

	m_Width = Width;
	m_Height = Height;
	str_copy(m_aName, pName);

	m_FPS = g_Config.m_ClVideoRecorderFPS;

	m_Recording = false;
	m_Started = false;
	m_ProcessingVideoFrame = 0;
	m_ProcessingAudioFrame = 0;

	m_HasAudio = g_Config.m_ClVideoSndEnable;

	dbg_assert(ms_pCurrentVideo == 0, "ms_pCurrentVideo is NOT set to NULL while creating a new Video.");

	ms_TickTime = time_freq() / m_FPS;
	ms_pCurrentVideo = this;
	g_WriteLock = lock_create();
}

CVideo::~CVideo()
{
	ms_pCurrentVideo = 0;
	lock_destroy(g_WriteLock);
}

void CVideo::Start()
{
	// wait for the graphic thread to idle
	m_pGraphics->WaitForIdle();

	m_AudioStream = {};
	m_VideoStream = {};

	char aDate[20];
	str_timestamp(aDate, sizeof(aDate));
	char aBuf[256];
	if(str_length(m_aName) != 0)
		str_format(aBuf, sizeof(aBuf), "videos/%s", m_aName);
	else
		str_format(aBuf, sizeof(aBuf), "videos/%s.mp4", aDate);

	char aWholePath[1024];
	IOHANDLE File = m_pStorage->OpenFile(aBuf, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath));

	if(File)
	{
		io_close(File);
	}
	else
	{
		dbg_msg("video_recorder", "Failed to open file for recoding video.");
		return;
	}
	avformat_alloc_output_context2(&m_pFormatContext, 0, "mp4", aWholePath);

	if(!m_pFormatContext)
	{
		dbg_msg("video_recorder", "Failed to create formatcontext for recoding video.");
		return;
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
	m_vPixelHelper.resize(m_VideoThreads);
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		m_vPixelHelper[i].resize(GLNVals * sizeof(uint8_t));
	}

	m_vBuffer.resize(m_AudioThreads);

	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if(m_pFormat->video_codec != AV_CODEC_ID_NONE)
	{
		if(!AddStream(&m_VideoStream, m_pFormatContext, &m_pVideoCodec, m_pFormat->video_codec))
			return;
	}
	else
	{
		dbg_msg("video_recorder", "Failed to add VideoStream for recoding video.");
	}

	if(m_HasAudio && m_pFormat->audio_codec != AV_CODEC_ID_NONE)
	{
		if(!AddStream(&m_AudioStream, m_pFormatContext, &m_pAudioCodec, m_pFormat->audio_codec))
			return;
	}
	else
	{
		dbg_msg("video_recorder", "No audio.");
	}

	m_vVideoThreads.resize(m_VideoThreads);
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		m_vVideoThreads[i] = std::make_unique<SVideoRecorderThread>();
	}
	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		std::unique_lock<std::mutex> Lock(m_vVideoThreads[i]->m_Mutex);
		m_vVideoThreads[i]->m_Thread = std::thread([this, i]() REQUIRES(!g_WriteLock) { RunVideoThread(i == 0 ? (m_VideoThreads - 1) : (i - 1), i); });
		m_vVideoThreads[i]->m_Cond.wait(Lock, [this, i]() -> bool { return m_vVideoThreads[i]->m_Started; });
	}

	m_vAudioThreads.resize(m_AudioThreads);
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		m_vAudioThreads[i] = std::make_unique<SAudioRecorderThread>();
	}
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		std::unique_lock<std::mutex> Lock(m_vAudioThreads[i]->m_Mutex);
		m_vAudioThreads[i]->m_Thread = std::thread([this, i]() REQUIRES(!g_WriteLock) { RunAudioThread(i == 0 ? (m_AudioThreads - 1) : (i - 1), i); });
		m_vAudioThreads[i]->m_Cond.wait(Lock, [this, i]() -> bool { return m_vAudioThreads[i]->m_Started; });
	}

	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if(!OpenVideo())
		return;

	if(m_HasAudio)
		if(!OpenAudio())
			return;

	// TODO: remove/comment:
	av_dump_format(m_pFormatContext, 0, aWholePath, 1);

	/* open the output file, if needed */
	if(!(m_pFormat->flags & AVFMT_NOFILE))
	{
		int Ret = avio_open(&m_pFormatContext->pb, aWholePath, AVIO_FLAG_WRITE);
		if(Ret < 0)
		{
			char aError[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(Ret, aError, sizeof(aError));
			dbg_msg("video_recorder", "Could not open '%s': %s", aWholePath, aError);
			return;
		}
	}

	m_VideoStream.m_vpSwsCtxs.reserve(m_VideoThreads);

	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		if(m_VideoStream.m_vpSwsCtxs.size() <= i)
			m_VideoStream.m_vpSwsCtxs.emplace_back(nullptr);

		if(!m_VideoStream.m_vpSwsCtxs[i])
		{
			m_VideoStream.m_vpSwsCtxs[i] = sws_getCachedContext(
				m_VideoStream.m_vpSwsCtxs[i],
				m_VideoStream.pEnc->width, m_VideoStream.pEnc->height, AV_PIX_FMT_RGBA,
				m_VideoStream.pEnc->width, m_VideoStream.pEnc->height, AV_PIX_FMT_YUV420P,
				0, 0, 0, 0);
		}
	}

	/* Write the stream header, if any. */
	int Ret = avformat_write_header(m_pFormatContext, &m_pOptDict);
	if(Ret < 0)
	{
		char aError[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Ret, aError, sizeof(aError));
		dbg_msg("video_recorder", "Error occurred when opening output file: %s", aError);
		return;
	}
	m_Recording = true;
	m_Started = true;
	ms_Time = time_get();
	m_Vframe = 0;
}

void CVideo::Pause(bool Pause)
{
	if(ms_pCurrentVideo)
		m_Recording = !Pause;
}

void CVideo::Stop()
{
	m_pGraphics->WaitForIdle();

	for(size_t i = 0; i < m_VideoThreads; ++i)
	{
		{
			std::unique_lock<std::mutex> Lock(m_vVideoThreads[i]->m_Mutex);
			m_vVideoThreads[i]->m_Finished = true;
			m_vVideoThreads[i]->m_Cond.notify_all();
		}

		m_vVideoThreads[i]->m_Thread.join();
	}
	m_vVideoThreads.clear();

	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		{
			std::unique_lock<std::mutex> Lock(m_vAudioThreads[i]->m_Mutex);
			m_vAudioThreads[i]->m_Finished = true;
			m_vAudioThreads[i]->m_Cond.notify_all();
		}

		m_vAudioThreads[i]->m_Thread.join();
	}
	m_vAudioThreads.clear();

	while(m_ProcessingVideoFrame > 0 || m_ProcessingAudioFrame > 0)
		std::this_thread::sleep_for(10us);

	m_Recording = false;

	FinishFrames(&m_VideoStream);

	if(m_HasAudio)
		FinishFrames(&m_AudioStream);

	av_write_trailer(m_pFormatContext);

	CloseStream(&m_VideoStream);

	if(m_HasAudio)
		CloseStream(&m_AudioStream);
	//fclose(m_dbgfile);

	if(!(m_pFormat->flags & AVFMT_NOFILE))
		avio_closep(&m_pFormatContext->pb);

	if(m_pFormatContext)
		avformat_free_context(m_pFormatContext);

	ISound *volatile pSound = m_pSound;

	pSound->PauseAudioDevice();
	delete ms_pCurrentVideo;
	pSound->UnpauseAudioDevice();
}

void CVideo::NextVideoFrameThread()
{
	if(m_Recording)
	{
		// #ifdef CONF_PLATFORM_MACOS
		// 	CAutoreleasePool AutoreleasePool;
		// #endif
		m_VSeq += 1;
		if(m_VSeq >= 2)
		{
			m_ProcessingVideoFrame.fetch_add(1);

			size_t NextVideoThreadIndex = m_CurVideoThreadIndex + 1;
			if(NextVideoThreadIndex == m_VideoThreads)
				NextVideoThreadIndex = 0;

			// always wait for the next video thread too, to prevent a dead lock

			{
				auto *pVideoThread = m_vVideoThreads[NextVideoThreadIndex].get();
				std::unique_lock<std::mutex> Lock(pVideoThread->m_Mutex);

				if(pVideoThread->m_HasVideoFrame)
				{
					pVideoThread->m_Cond.wait(Lock, [&pVideoThread]() -> bool { return !pVideoThread->m_HasVideoFrame; });
				}
			}

			//dbg_msg("video_recorder", "vframe: %d", m_VideoStream.pEnc->frame_number);

			// after reading the graphic libraries' frame buffer, go threaded
			{
				auto *pVideoThread = m_vVideoThreads[m_CurVideoThreadIndex].get();
				std::unique_lock<std::mutex> Lock(pVideoThread->m_Mutex);

				if(pVideoThread->m_HasVideoFrame)
				{
					pVideoThread->m_Cond.wait(Lock, [&pVideoThread]() -> bool { return !pVideoThread->m_HasVideoFrame; });
				}

				ReadRGBFromGL(m_CurVideoThreadIndex);

				pVideoThread->m_HasVideoFrame = true;
				{
					std::unique_lock<std::mutex> LockParent(pVideoThread->m_VideoFillMutex);
					pVideoThread->m_VideoFrameToFill = m_VSeq;
				}
				pVideoThread->m_Cond.notify_all();
			}

			++m_CurVideoThreadIndex;
			if(m_CurVideoThreadIndex == m_VideoThreads)
				m_CurVideoThreadIndex = 0;
		}

		// sync_barrier();
		// m_Semaphore.signal();
	}
}

void CVideo::NextVideoFrame()
{
	if(m_Recording)
	{
		ms_Time += ms_TickTime;
		ms_LocalTime = (ms_Time - ms_LocalStartTime) / (float)time_freq();
		m_Vframe += 1;
	}
}

void CVideo::NextAudioFrameTimeline(ISoundMixFunc Mix)
{
	if(m_Recording && m_HasAudio)
	{
		//if(m_VideoStream.pEnc->frame_number * (double)m_AudioStream.pEnc->sample_rate / m_FPS >= (double)m_AudioStream.pEnc->frame_number * m_AudioStream.pEnc->frame_size)
		double SamplesPerFrame = (double)m_AudioStream.pEnc->sample_rate / m_FPS;
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
		m_ASeq += 1;

		m_ProcessingAudioFrame.fetch_add(1);

		size_t NextAudioThreadIndex = m_CurAudioThreadIndex + 1;
		if(NextAudioThreadIndex == m_AudioThreads)
			NextAudioThreadIndex = 0;

		// always wait for the next Audio thread too, to prevent a dead lock

		{
			auto *pAudioThread = m_vAudioThreads[NextAudioThreadIndex].get();
			std::unique_lock<std::mutex> Lock(pAudioThread->m_Mutex);

			if(pAudioThread->m_HasAudioFrame)
			{
				pAudioThread->m_Cond.wait(Lock, [&pAudioThread]() -> bool { return !pAudioThread->m_HasAudioFrame; });
			}
		}

		// after reading the graphic libraries' frame buffer, go threaded
		{
			auto *pAudioThread = m_vAudioThreads[m_CurAudioThreadIndex].get();

			std::unique_lock<std::mutex> Lock(pAudioThread->m_Mutex);

			if(pAudioThread->m_HasAudioFrame)
			{
				pAudioThread->m_Cond.wait(Lock, [&pAudioThread]() -> bool { return !pAudioThread->m_HasAudioFrame; });
			}

			Mix(m_vBuffer[m_CurAudioThreadIndex].m_aBuffer, ALEN / 2); // two channels

			int64_t DstNbSamples = av_rescale_rnd(
				swr_get_delay(m_AudioStream.m_vpSwrCtxs[m_CurAudioThreadIndex], m_AudioStream.pEnc->sample_rate) +
					m_AudioStream.m_vpFrames[m_CurAudioThreadIndex]->nb_samples,
				m_AudioStream.pEnc->sample_rate,
				m_AudioStream.pEnc->sample_rate, AV_ROUND_UP);

			pAudioThread->m_SampleCountStart = m_AudioStream.m_SamplesCount;
			m_AudioStream.m_SamplesCount += DstNbSamples;

			pAudioThread->m_HasAudioFrame = true;
			{
				std::unique_lock<std::mutex> LockParent(pAudioThread->m_AudioFillMutex);
				pAudioThread->m_AudioFrameToFill = m_ASeq;
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
	auto *pThreadData = m_vAudioThreads[ThreadIndex].get();
	auto *pParentThreadData = m_vAudioThreads[ParentThreadIndex].get();
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
					m_AudioStream.m_vpFrames[ThreadIndex]->pts = av_rescale_q(pThreadData->m_SampleCountStart, AVRational{1, m_AudioStream.pEnc->sample_rate}, m_AudioStream.pEnc->time_base);
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
	av_samples_fill_arrays(
		(uint8_t **)m_AudioStream.m_vpTmpFrames[ThreadIndex]->data,
		0, // pointer to linesize (int*)
		(const uint8_t *)m_vBuffer[ThreadIndex].m_aBuffer,
		2, // channels
		m_AudioStream.m_vpTmpFrames[ThreadIndex]->nb_samples,
		AV_SAMPLE_FMT_S16,
		0 // align
	);

	// dbg_msg("video_recorder", "DstNbSamples: %d", DstNbSamples);
	// fwrite(m_aBuffer, sizeof(short), 2048, m_dbgfile);

	int Ret = av_frame_make_writable(m_AudioStream.m_vpFrames[ThreadIndex]);
	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Error making frame writable");
		return;
	}

	/* convert to destination format */
	Ret = swr_convert(
		m_AudioStream.m_vpSwrCtxs[ThreadIndex],
		m_AudioStream.m_vpFrames[ThreadIndex]->data,
		m_AudioStream.m_vpFrames[ThreadIndex]->nb_samples,
		(const uint8_t **)m_AudioStream.m_vpTmpFrames[ThreadIndex]->data,
		m_AudioStream.m_vpTmpFrames[ThreadIndex]->nb_samples);

	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Error while converting");
		return;
	}
}

void CVideo::RunVideoThread(size_t ParentThreadIndex, size_t ThreadIndex)
{
	auto *pThreadData = m_vVideoThreads[ThreadIndex].get();
	auto *pParentThreadData = m_vVideoThreads[ParentThreadIndex].get();
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
					m_VideoStream.m_vpFrames[ThreadIndex]->pts = (int64_t)m_VideoStream.pEnc->frame_number;
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
	const int InLineSize = 4 * m_VideoStream.pEnc->width;
	auto *pRGBAData = m_vPixelHelper[ThreadIndex].data();
	sws_scale(m_VideoStream.m_vpSwsCtxs[ThreadIndex], (const uint8_t *const *)&pRGBAData, &InLineSize, 0,
		m_VideoStream.pEnc->height, m_VideoStream.m_vpFrames[ThreadIndex]->data, m_VideoStream.m_vpFrames[ThreadIndex]->linesize);
}

void CVideo::ReadRGBFromGL(size_t ThreadIndex)
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Format;
	m_pGraphics->GetReadPresentedImageDataFuncUnsafe()(Width, Height, Format, m_vPixelHelper[ThreadIndex]);
}

AVFrame *CVideo::AllocPicture(enum AVPixelFormat PixFmt, int Width, int Height)
{
	AVFrame *pPicture;
	int Ret;

	pPicture = av_frame_alloc();
	if(!pPicture)
		return NULL;

	pPicture->format = PixFmt;
	pPicture->width = Width;
	pPicture->height = Height;

	/* allocate the buffers for the frame data */
	Ret = av_frame_get_buffer(pPicture, 32);
	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Could not allocate frame data.");
		return nullptr;
	}

	return pPicture;
}

AVFrame *CVideo::AllocAudioFrame(enum AVSampleFormat SampleFmt, uint64_t ChannelLayout, int SampleRate, int NbSamples)
{
	AVFrame *pFrame = av_frame_alloc();
	int Ret;

	if(!pFrame)
	{
		dbg_msg("video_recorder", "Error allocating an audio frame");
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
		Ret = av_frame_get_buffer(pFrame, 0);
		if(Ret < 0)
		{
			dbg_msg("video_recorder", "Error allocating an audio buffer");
			return nullptr;
		}
	}

	return pFrame;
}

bool CVideo::OpenVideo()
{
	int Ret;
	AVCodecContext *pContext = m_VideoStream.pEnc;
	AVDictionary *pOptions = 0;

	av_dict_copy(&pOptions, m_pOptDict, 0);

	/* open the codec */
	Ret = avcodec_open2(pContext, m_pVideoCodec, &pOptions);
	av_dict_free(&pOptions);
	if(Ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Could not open video codec: %s", aBuf);
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
			dbg_msg("video_recorder", "Could not allocate video frame");
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
				dbg_msg("video_recorder", "Could not allocate temporary video frame");
				return false;
			}
		}
	}

	/* copy the stream parameters to the muxer */
	Ret = avcodec_parameters_from_context(m_VideoStream.pSt->codecpar, pContext);
	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Could not copy the stream parameters");
		return false;
	}
	m_VSeq = 0;
	return true;
}

bool CVideo::OpenAudio()
{
	AVCodecContext *pContext;
	int NbSamples;
	int Ret;
	AVDictionary *pOptions = NULL;

	pContext = m_AudioStream.pEnc;

	/* open it */
	//m_dbgfile = fopen("/tmp/pcm_dbg", "wb");
	av_dict_copy(&pOptions, m_pOptDict, 0);
	Ret = avcodec_open2(pContext, m_pAudioCodec, &pOptions);
	av_dict_free(&pOptions);
	if(Ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Could not open audio codec: %s", aBuf);
		return false;
	}

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
			dbg_msg("video_recorder", "Could not allocate audio frame");
			return false;
		}

		m_AudioStream.m_vpTmpFrames.emplace_back(nullptr);
		m_AudioStream.m_vpTmpFrames[i] = AllocAudioFrame(AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_STEREO, g_Config.m_SndRate, NbSamples);
		if(!m_AudioStream.m_vpTmpFrames[i])
		{
			dbg_msg("video_recorder", "Could not allocate audio frame");
			return false;
		}
	}

	/* copy the stream parameters to the muxer */
	Ret = avcodec_parameters_from_context(m_AudioStream.pSt->codecpar, pContext);
	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Could not copy the stream parameters");
		return false;
	}

	/* create resampler context */
	m_AudioStream.m_vpSwrCtxs.clear();
	m_AudioStream.m_vpSwrCtxs.resize(m_AudioThreads);
	for(size_t i = 0; i < m_AudioThreads; ++i)
	{
		m_AudioStream.m_vpSwrCtxs[i] = swr_alloc();
		if(!m_AudioStream.m_vpSwrCtxs[i])
		{
			dbg_msg("video_recorder", "Could not allocate resampler context");
			return false;
		}

		/* set options */
		av_opt_set_int(m_AudioStream.m_vpSwrCtxs[i], "in_channel_count", 2, 0);
		av_opt_set_int(m_AudioStream.m_vpSwrCtxs[i], "in_sample_rate", g_Config.m_SndRate, 0);
		av_opt_set_sample_fmt(m_AudioStream.m_vpSwrCtxs[i], "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
		av_opt_set_int(m_AudioStream.m_vpSwrCtxs[i], "out_channel_count", pContext->ch_layout.nb_channels, 0);
#else
		av_opt_set_int(m_AudioStream.m_vpSwrCtxs[i], "out_channel_count", pContext->channels, 0);
#endif
		av_opt_set_int(m_AudioStream.m_vpSwrCtxs[i], "out_sample_rate", pContext->sample_rate, 0);
		av_opt_set_sample_fmt(m_AudioStream.m_vpSwrCtxs[i], "out_sample_fmt", pContext->sample_fmt, 0);

		/* initialize the resampling context */
		if(swr_init(m_AudioStream.m_vpSwrCtxs[i]) < 0)
		{
			dbg_msg("video_recorder", "Failed to initialize the resampling context");
			return false;
		}
	}

	m_ASeq = 0;
	return true;
}

/* Add an output stream. */
bool CVideo::AddStream(OutputStream *pStream, AVFormatContext *pOC, const AVCodec **ppCodec, enum AVCodecID CodecId)
{
	AVCodecContext *pContext;

	/* find the encoder */
	*ppCodec = avcodec_find_encoder(CodecId);
	if(!(*ppCodec))
	{
		dbg_msg("video_recorder", "Could not find encoder for '%s'",
			avcodec_get_name(CodecId));
		return false;
	}

	pStream->pSt = avformat_new_stream(pOC, NULL);
	if(!pStream->pSt)
	{
		dbg_msg("video_recorder", "Could not allocate stream");
		return false;
	}
	pStream->pSt->id = pOC->nb_streams - 1;
	pContext = avcodec_alloc_context3(*ppCodec);
	if(!pContext)
	{
		dbg_msg("video_recorder", "Could not alloc an encoding context");
		return false;
	}
	pStream->pEnc = pContext;

#if defined(CONF_ARCH_IA32) || defined(CONF_ARCH_ARM)
	// use only 1 ffmpeg thread on 32-bit to save memory
	pContext->thread_count = 1;
#endif

	switch((*ppCodec)->type)
	{
	case AVMEDIA_TYPE_AUDIO:
		pContext->sample_fmt = (*ppCodec)->sample_fmts ? (*ppCodec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		pContext->bit_rate = g_Config.m_SndRate * 2 * 16;
		pContext->sample_rate = g_Config.m_SndRate;
		if((*ppCodec)->supported_samplerates)
		{
			pContext->sample_rate = (*ppCodec)->supported_samplerates[0];
			for(int i = 0; (*ppCodec)->supported_samplerates[i]; i++)
			{
				if((*ppCodec)->supported_samplerates[i] == g_Config.m_SndRate)
				{
					pContext->sample_rate = g_Config.m_SndRate;
					break;
				}
			}
		}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
		dbg_assert(av_channel_layout_from_mask(&pContext->ch_layout, AV_CH_LAYOUT_STEREO) == 0, "Failed to set channel layout");
#else
		pContext->channels = 2;
		pContext->channel_layout = AV_CH_LAYOUT_STEREO;
#endif

		pStream->pSt->time_base.num = 1;
		pStream->pSt->time_base.den = pContext->sample_rate;
		break;

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
		pStream->pSt->time_base.num = 1;
		pStream->pSt->time_base.den = m_FPS;
		pContext->time_base = pStream->pSt->time_base;

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
			av_opt_set(pContext->priv_data, "preset", s_apPresets[g_Config.m_ClVideoX264Preset], 0);
			av_opt_set_int(pContext->priv_data, "crf", g_Config.m_ClVideoX264Crf, 0);
		}
		break;

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if(pOC->oformat->flags & AVFMT_GLOBALHEADER)
		pContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return true;
}

void CVideo::WriteFrame(OutputStream *pStream, size_t ThreadIndex)
{
	int RetRecv = 0;

	AVPacket *pPacket = av_packet_alloc();
	if(pPacket == nullptr)
	{
		dbg_msg("video_recorder", "Failed allocating packet");
		return;
	}

	pPacket->data = 0;
	pPacket->size = 0;

	avcodec_send_frame(pStream->pEnc, pStream->m_vpFrames[ThreadIndex]);
	do
	{
		RetRecv = avcodec_receive_packet(pStream->pEnc, pPacket);
		if(!RetRecv)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(pPacket, pStream->pEnc->time_base, pStream->pSt->time_base);
			pPacket->stream_index = pStream->pSt->index;

			if(int Ret = av_interleaved_write_frame(m_pFormatContext, pPacket))
			{
				char aBuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(Ret, aBuf, sizeof(aBuf));
				dbg_msg("video_recorder", "Error while writing video frame: %s", aBuf);
			}
		}
		else
			break;
	} while(true);

	if(RetRecv && RetRecv != AVERROR(EAGAIN))
	{
		dbg_msg("video_recorder", "Error encoding frame, error: %d", RetRecv);
	}

	av_packet_free(&pPacket);
}

void CVideo::FinishFrames(OutputStream *pStream)
{
	dbg_msg("video_recorder", "------------");
	int RetRecv = 0;

	AVPacket *pPacket = av_packet_alloc();
	if(pPacket == nullptr)
	{
		dbg_msg("video_recorder", "Failed allocating packet");
		return;
	}

	pPacket->data = 0;
	pPacket->size = 0;

	avcodec_send_frame(pStream->pEnc, 0);
	do
	{
		RetRecv = avcodec_receive_packet(pStream->pEnc, pPacket);
		if(!RetRecv)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(pPacket, pStream->pEnc->time_base, pStream->pSt->time_base);
			pPacket->stream_index = pStream->pSt->index;

			if(int Ret = av_interleaved_write_frame(m_pFormatContext, pPacket))
			{
				char aBuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(Ret, aBuf, sizeof(aBuf));
				dbg_msg("video_recorder", "Error while writing video frame: %s", aBuf);
			}
		}
		else
			break;
	} while(true);

	if(RetRecv && RetRecv != AVERROR_EOF)
	{
		dbg_msg("video_recorder", "failed to finish recoding, error: %d", RetRecv);
	}

	av_packet_free(&pPacket);
}

void CVideo::CloseStream(OutputStream *pStream)
{
	avcodec_free_context(&pStream->pEnc);
	for(auto *pFrame : pStream->m_vpFrames)
		av_frame_free(&pFrame);
	pStream->m_vpFrames.clear();

	for(auto *pFrame : pStream->m_vpTmpFrames)
		av_frame_free(&pFrame);
	pStream->m_vpTmpFrames.clear();

	for(auto *pSwsContext : pStream->m_vpSwsCtxs)
		sws_freeContext(pSwsContext);
	pStream->m_vpSwsCtxs.clear();

	for(auto *pSwrContext : pStream->m_vpSwrCtxs)
		swr_free(&pSwrContext);
	pStream->m_vpSwrCtxs.clear();
}

#endif
