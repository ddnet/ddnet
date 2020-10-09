#if defined(CONF_VIDEORECORDER)

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "video.h"

// This code is mostly stolen from https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/muxing.c

#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */

const size_t FORMAT_NCHANNELS = 3;
static LOCK g_WriteLock = 0;

CVideo::CVideo(CGraphics_Threaded *pGraphics, IStorage *pStorage, IConsole *pConsole, int Width, int Height, const char *pName) :
	m_pGraphics(pGraphics),
	m_pStorage(pStorage),
	m_pConsole(pConsole),
	m_VideoStream(),
	m_AudioStream()
{
	m_pPixels = 0;

	m_pFormatContext = 0;
	m_pFormat = 0;
	m_pRGB = 0;
	m_pOptDict = 0;

	m_VideoCodec = 0;
	m_AudioCodec = 0;

	m_Width = Width;
	m_Height = Height;
	str_copy(m_Name, pName, sizeof(m_Name));

	m_FPS = g_Config.m_ClVideoRecorderFPS;

	m_Recording = false;
	m_Started = false;
	m_ProcessingVideoFrame = false;
	m_ProcessingAudioFrame = false;

	m_NextFrame = false;
	m_NextAudioFrame = false;

	// TODO:
	m_HasAudio = g_Config.m_ClVideoSndEnable;

	m_SndBufferSize = g_Config.m_SndBufferSize;

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
	char aDate[20];
	str_timestamp(aDate, sizeof(aDate));
	char aBuf[256];
	if(strlen(m_Name) != 0)
		str_format(aBuf, sizeof(aBuf), "videos/%s", m_Name);
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

	size_t NVals = FORMAT_NCHANNELS * m_Width * m_Height;
	m_pPixels = (uint8_t *)malloc(NVals * sizeof(GLubyte));
	m_pRGB = (uint8_t *)malloc(NVals * sizeof(uint8_t));

	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if(m_pFormat->video_codec != AV_CODEC_ID_NONE)
	{
		if(!AddStream(&m_VideoStream, m_pFormatContext, &m_VideoCodec, m_pFormat->video_codec))
			return;
	}
	else
	{
		dbg_msg("video_recorder", "Failed to add VideoStream for recoding video.");
	}

	if(m_HasAudio && m_pFormat->audio_codec != AV_CODEC_ID_NONE)
	{
		if(!AddStream(&m_AudioStream, m_pFormatContext, &m_AudioCodec, m_pFormat->audio_codec))
			return;
	}
	else
	{
		dbg_msg("video_recorder", "No audio.");
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
			char aBuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(Ret, aBuf, sizeof(aBuf));
			dbg_msg("video_recorder", "Could not open '%s': %s", aWholePath, aBuf);
			return;
		}
	}

	if(!m_VideoStream.pSwsCtx)
	{
		m_VideoStream.pSwsCtx = sws_getCachedContext(
			m_VideoStream.pSwsCtx,
			m_VideoStream.pEnc->width, m_VideoStream.pEnc->height, AV_PIX_FMT_RGB24,
			m_VideoStream.pEnc->width, m_VideoStream.pEnc->height, AV_PIX_FMT_YUV420P,
			0, 0, 0, 0);
	}

	/* Write the stream header, if any. */
	int Ret = avformat_write_header(m_pFormatContext, &m_pOptDict);
	if(Ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Error occurred when opening output file: %s", aBuf);
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
	m_Recording = false;

	while(m_ProcessingVideoFrame || m_ProcessingAudioFrame)
		thread_sleep(10);

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

	if(m_pRGB)
		free(m_pRGB);

	if(m_pPixels)
		free(m_pPixels);

	if(ms_pCurrentVideo)
		delete ms_pCurrentVideo;
}

void CVideo::NextVideoFrameThread()
{
	if(m_NextFrame && m_Recording)
	{
		// #ifdef CONF_PLATFORM_MACOSX
		// 	CAutoreleasePool AutoreleasePool;
		// #endif
		m_Vseq += 1;
		if(m_Vseq >= 2)
		{
			m_ProcessingVideoFrame = true;
			m_VideoStream.pFrame->pts = (int64)m_VideoStream.pEnc->frame_number;
			//dbg_msg("video_recorder", "vframe: %d", m_VideoStream.pEnc->frame_number);

			ReadRGBFromGL();
			FillVideoFrame();
			lock_wait(g_WriteLock);
			WriteFrame(&m_VideoStream);
			lock_unlock(g_WriteLock);
			m_ProcessingVideoFrame = false;
		}

		m_NextFrame = false;
		// sync_barrier();
		// m_Semaphore.signal();
	}
}

void CVideo::NextVideoFrame()
{
	if(m_Recording)
	{
		// #ifdef CONF_PLATFORM_MACOSX
		// 	CAutoreleasePool AutoreleasePool;
		// #endif

		//dbg_msg("video_recorder", "called");

		ms_Time += ms_TickTime;
		ms_LocalTime = (ms_Time - ms_LocalStartTime) / (float)time_freq();
		m_NextFrame = true;
		m_Vframe += 1;

		// m_pGraphics->KickCommandBuffer();
		//thread_sleep(500);

		// m_Semaphore.wait();
	}
}

void CVideo::NextAudioFrameTimeline()
{
	if(m_Recording && m_HasAudio)
	{
		//if(m_Vframe * m_AudioStream.pEnc->sample_rate / m_FPS >= m_AudioStream.pEnc->frame_number*m_AudioStream.pEnc->frame_size)
		if(m_VideoStream.pEnc->frame_number * (double)m_AudioStream.pEnc->sample_rate / m_FPS >= (double)m_AudioStream.pEnc->frame_number * m_AudioStream.pEnc->frame_size)
		{
			m_NextAudioFrame = true;
		}
	}
}

void CVideo::NextAudioFrame(void (*Mix)(short *pFinalOut, unsigned Frames))
{
	if(m_NextAudioFrame && m_Recording && m_HasAudio)
	{
		m_ProcessingAudioFrame = true;
		//dbg_msg("video recorder", "video_frame: %lf", (double)(m_Vframe/m_FPS));
		//if((double)(m_Vframe/m_FPS) < m_AudioStream.pEnc->frame_number*m_AudioStream.pEnc->frame_size/m_AudioStream.pEnc->sample_rate)
		//return;
		Mix(m_aBuffer, ALEN);
		//m_AudioStream.pFrame->pts = m_AudioStream.pEnc->frame_number;
		//dbg_msg("video_recorder", "aframe: %d", m_AudioStream.pEnc->frame_number);

		// memcpy(m_AudioStream.pTmpFrame->data[0], pData, sizeof(int16_t) * m_SndBufferSize * 2);
		//
		// for(int i = 0; i < m_SndBufferSize; i++)
		// {
		// 	dbg_msg("video_recorder", "test: %d %d", ((int16_t*)pData)[i*2], ((int16_t*)pData)[i*2 + 1]);
		// }

		int DstNbSamples;

		av_samples_fill_arrays(
			(uint8_t **)m_AudioStream.pTmpFrame->data,
			0, // pointer to linesize (int*)
			(const uint8_t *)m_aBuffer,
			2, // channels
			m_AudioStream.pTmpFrame->nb_samples,
			AV_SAMPLE_FMT_S16,
			0 // align
		);

		DstNbSamples = av_rescale_rnd(
			swr_get_delay(
				m_AudioStream.pSwrCtx,
				m_AudioStream.pEnc->sample_rate) +
				m_AudioStream.pTmpFrame->nb_samples,

			m_AudioStream.pEnc->sample_rate,
			m_AudioStream.pEnc->sample_rate, AV_ROUND_UP);

		// dbg_msg("video_recorder", "DstNbSamples: %d", DstNbSamples);
		// fwrite(m_aBuffer, sizeof(short), 2048, m_dbgfile);

		int Ret = av_frame_make_writable(m_AudioStream.pFrame);
		if(Ret < 0)
		{
			dbg_msg("video_recorder", "Error making frame writable");
			return;
		}

		/* convert to destination format */
		Ret = swr_convert(
			m_AudioStream.pSwrCtx,
			m_AudioStream.pFrame->data,
			m_AudioStream.pFrame->nb_samples,
			(const uint8_t **)m_AudioStream.pTmpFrame->data,
			m_AudioStream.pTmpFrame->nb_samples);

		if(Ret < 0)
		{
			dbg_msg("video_recorder", "Error while converting");
			return;
		}

		// frame = ost->frame;
		//
		m_AudioStream.pFrame->pts = av_rescale_q(m_AudioStream.SamplesCount, AVRational{1, m_AudioStream.pEnc->sample_rate}, m_AudioStream.pEnc->time_base);
		m_AudioStream.SamplesCount += DstNbSamples;

		// dbg_msg("video_recorder", "prewrite----");
		lock_wait(g_WriteLock);
		WriteFrame(&m_AudioStream);
		lock_unlock(g_WriteLock);

		m_ProcessingAudioFrame = false;
		m_NextAudioFrame = false;
	}
}

void CVideo::FillAudioFrame()
{
}

void CVideo::FillVideoFrame()
{
	const int InLinesize[1] = {3 * m_VideoStream.pEnc->width};
	sws_scale(m_VideoStream.pSwsCtx, (const uint8_t *const *)&m_pRGB, InLinesize, 0,
		m_VideoStream.pEnc->height, m_VideoStream.pFrame->data, m_VideoStream.pFrame->linesize);
}

void CVideo::ReadRGBFromGL()
{
	/* Get RGBA to align to 32 bits instead of just 24 for RGB. May be faster for FFmpeg. */
	glReadBuffer(GL_FRONT);
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, m_Width, m_Height, GL_RGB, GL_UNSIGNED_BYTE, m_pPixels);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);
	for(int i = 0; i < m_Height; i++)
	{
		for(int j = 0; j < m_Width; j++)
		{
			size_t CurGL = FORMAT_NCHANNELS * (m_Width * (m_Height - i - 1) + j);
			size_t CurRGB = FORMAT_NCHANNELS * (m_Width * i + j);
			for(int k = 0; k < (int)FORMAT_NCHANNELS; k++)
				m_pRGB[CurRGB + k] = m_pPixels[CurGL + k];
		}
	}
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

AVFrame *CVideo::AllocAudioFrame(enum AVSampleFormat SampleFmt, uint64 ChannelLayout, int SampleRate, int NbSamples)
{
	AVFrame *Frame = av_frame_alloc();
	int Ret;

	if(!Frame)
	{
		dbg_msg("video_recorder", "Error allocating an audio frame");
		return nullptr;
	}

	Frame->format = SampleFmt;
	Frame->channel_layout = ChannelLayout;
	Frame->sample_rate = SampleRate;
	Frame->nb_samples = NbSamples;

	if(NbSamples)
	{
		Ret = av_frame_get_buffer(Frame, 0);
		if(Ret < 0)
		{
			dbg_msg("video_recorder", "Error allocating an audio buffer");
			return nullptr;
		}
	}

	return Frame;
}

bool CVideo::OpenVideo()
{
	int Ret;
	AVCodecContext *c = m_VideoStream.pEnc;
	AVDictionary *opt = 0;

	av_dict_copy(&opt, m_pOptDict, 0);

	/* open the codec */
	Ret = avcodec_open2(c, m_VideoCodec, &opt);
	av_dict_free(&opt);
	if(Ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Could not open video codec: %s", aBuf);
		return false;
	}

	/* allocate and init a re-usable frame */
	m_VideoStream.pFrame = AllocPicture(c->pix_fmt, c->width, c->height);
	if(!m_VideoStream.pFrame)
	{
		dbg_msg("video_recorder", "Could not allocate video frame");
		return false;
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	m_VideoStream.pTmpFrame = NULL;
	if(c->pix_fmt != AV_PIX_FMT_YUV420P)
	{
		m_VideoStream.pTmpFrame = AllocPicture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if(!m_VideoStream.pTmpFrame)
		{
			dbg_msg("video_recorder", "Could not allocate temporary picture");
			return false;
		}
	}

	/* copy the stream parameters to the muxer */
	Ret = avcodec_parameters_from_context(m_VideoStream.pSt->codecpar, c);
	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Could not copy the stream parameters");
		return false;
	}
	m_Vseq = 0;
	return true;
}

bool CVideo::OpenAudio()
{
	AVCodecContext *c;
	int NbSamples;
	int Ret;
	AVDictionary *opt = NULL;

	c = m_AudioStream.pEnc;

	/* open it */
	//m_dbgfile = fopen("/tmp/pcm_dbg", "wb");
	av_dict_copy(&opt, m_pOptDict, 0);
	Ret = avcodec_open2(c, m_AudioCodec, &opt);
	av_dict_free(&opt);
	if(Ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(Ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Could not open audio codec: %s", aBuf);
		return false;
	}

	if(c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		NbSamples = 10000;
	else
		NbSamples = c->frame_size;

	m_AudioStream.pFrame = AllocAudioFrame(c->sample_fmt, c->channel_layout, c->sample_rate, NbSamples);

	m_AudioStream.pTmpFrame = AllocAudioFrame(AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_STEREO, g_Config.m_SndRate, m_SndBufferSize * 2);

	/* copy the stream parameters to the muxer */
	Ret = avcodec_parameters_from_context(m_AudioStream.pSt->codecpar, c);
	if(Ret < 0)
	{
		dbg_msg("video_recorder", "Could not copy the stream parameters");
		return false;
	}

	/* create resampler context */
	m_AudioStream.pSwrCtx = swr_alloc();
	if(!m_AudioStream.pSwrCtx)
	{
		dbg_msg("video_recorder", "Could not allocate resampler context");
		return false;
	}

	/* set options */
	av_opt_set_int(m_AudioStream.pSwrCtx, "in_channel_count", 2, 0);
	av_opt_set_int(m_AudioStream.pSwrCtx, "in_sample_rate", g_Config.m_SndRate, 0);
	av_opt_set_sample_fmt(m_AudioStream.pSwrCtx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(m_AudioStream.pSwrCtx, "out_channel_count", c->channels, 0);
	av_opt_set_int(m_AudioStream.pSwrCtx, "out_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(m_AudioStream.pSwrCtx, "out_sample_fmt", c->sample_fmt, 0);

	/* initialize the resampling context */
	if((Ret = swr_init(m_AudioStream.pSwrCtx)) < 0)
	{
		dbg_msg("video_recorder", "Failed to initialize the resampling context");
		return false;
	}

	return true;
}

/* Add an output stream. */
bool CVideo::AddStream(OutputStream *pStream, AVFormatContext *pOC, AVCodec **ppCodec, enum AVCodecID CodecId)
{
	AVCodecContext *c;

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
	c = avcodec_alloc_context3(*ppCodec);
	if(!c)
	{
		dbg_msg("video_recorder", "Could not alloc an encoding context");
		return false;
	}
	pStream->pEnc = c;

	switch((*ppCodec)->type)
	{
	case AVMEDIA_TYPE_AUDIO:

		// m_MixingRate = g_Config.m_SndRate;
		//
		// // Set 16-bit stereo audio at 22Khz
		// Format.freq = g_Config.m_SndRate; // ignore_convention
		// Format.format = AUDIO_S16; // ignore_convention
		// Format.channels = 2; // ignore_convention
		// Format.samples = g_Config.m_SndBufferSize; // ignore_convention

		c->sample_fmt = (*ppCodec)->sample_fmts ? (*ppCodec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = g_Config.m_SndRate * 2 * 16;
		c->frame_size = m_SndBufferSize;
		c->sample_rate = g_Config.m_SndRate;
		if((*ppCodec)->supported_samplerates)
		{
			c->sample_rate = (*ppCodec)->supported_samplerates[0];
			for(int i = 0; (*ppCodec)->supported_samplerates[i]; i++)
			{
				if((*ppCodec)->supported_samplerates[i] == g_Config.m_SndRate)
					c->sample_rate = g_Config.m_SndRate;
			}
		}
		c->channels = 2;
		c->channel_layout = AV_CH_LAYOUT_STEREO;

		pStream->pSt->time_base.num = 1;
		pStream->pSt->time_base.den = c->sample_rate;
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = CodecId;

		c->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		c->width = m_Width;
		c->height = m_Height % 2 == 0 ? m_Height : m_Height - 1;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
			 * of which frame timestamps are represented. For fixed-fps content,
			 * timebase should be 1/framerate and timestamp increments should be
			 * identical to 1. */
		pStream->pSt->time_base.num = 1;
		pStream->pSt->time_base.den = m_FPS;
		c->time_base = pStream->pSt->time_base;

		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if(c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
		{
			/* just for testing, we also add B-frames */
			c->max_b_frames = 2;
		}
		if(c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
		{
			/* Needed to avoid using macroblocks in which some coeffs overflow.
				 * This does not happen with normal video, it just happens here as
				 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		if(CodecId == AV_CODEC_ID_H264)
		{
			const char *presets[10] = {"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow", "placebo"};
			//av_opt_set(c->priv_data, "preset", "slow", 0);
			//av_opt_set_int(c->priv_data, "crf", 22, 0);
			av_opt_set(c->priv_data, "preset", presets[g_Config.m_ClVideoX264Preset], 0);
			av_opt_set_int(c->priv_data, "crf", g_Config.m_ClVideoX264Crf, 0);
		}
		break;

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if(pOC->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return true;
}

void CVideo::WriteFrame(OutputStream *pStream)
{
	//lock_wait(g_WriteLock);
	int RetRecv = 0;

	AVPacket Packet = {0};

	av_init_packet(&Packet);
	Packet.data = 0;
	Packet.size = 0;

	avcodec_send_frame(pStream->pEnc, pStream->pFrame);
	do
	{
		RetRecv = avcodec_receive_packet(pStream->pEnc, &Packet);
		if(!RetRecv)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(&Packet, pStream->pEnc->time_base, pStream->pSt->time_base);
			Packet.stream_index = pStream->pSt->index;

			if(int Ret = av_interleaved_write_frame(m_pFormatContext, &Packet))
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
	//lock_unlock(g_WriteLock);
}

void CVideo::FinishFrames(OutputStream *pStream)
{
	dbg_msg("video_recorder", "------------");
	int RetRecv = 0;

	AVPacket Packet = {0};

	av_init_packet(&Packet);
	Packet.data = 0;
	Packet.size = 0;

	avcodec_send_frame(pStream->pEnc, 0);
	do
	{
		RetRecv = avcodec_receive_packet(pStream->pEnc, &Packet);
		if(!RetRecv)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			//if(pStream->pSt->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			av_packet_rescale_ts(&Packet, pStream->pEnc->time_base, pStream->pSt->time_base);
			Packet.stream_index = pStream->pSt->index;

			if(int Ret = av_interleaved_write_frame(m_pFormatContext, &Packet))
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
}

void CVideo::CloseStream(OutputStream *pStream)
{
	avcodec_free_context(&pStream->pEnc);
	av_frame_free(&pStream->pFrame);
	av_frame_free(&pStream->pTmpFrame);
	sws_freeContext(pStream->pSwsCtx);
	swr_free(&pStream->pSwrCtx);
}

#endif
