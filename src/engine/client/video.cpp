#if defined(CONF_VIDEORECORDER)

#include <engine/storage.h>
#include <engine/console.h>
#include <engine/shared/config.h>

#include "video.h"

// This code is mostly stolen from https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/muxing.c

#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */

CVideo::CVideo(CGraphics_Threaded* pGraphics, IStorage* pStorage, IConsole *pConsole, int width, int height) :
	IVideo::IVideo(),
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

	m_Width = width;
	m_Height = height;

	m_FPS = g_Config.m_ClVideoRecorderFPS;

	m_Recording = false;
	m_Started = false;
	m_ProcessingVideoFrame = false;
	m_ProcessingAudioFrame = false;

	m_NextFrame = false;

	// TODO:
	m_HasAudio = false;

	m_SndBufferSize =  g_Config.m_SndBufferSize;

	dbg_assert(ms_pCurrentVideo == 0, "ms_pCurrentVideo is NOT set to NULL while creating a new Video.");

	ms_TickTime = time_freq() / m_FPS;
	ms_pCurrentVideo = this;
}

CVideo::~CVideo()
{
	ms_pCurrentVideo = 0;
}

void CVideo::start()
{
	char aDate[20];
	str_timestamp(aDate, sizeof(aDate));
	char aBuf[64];
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

	if (!m_pFormatContext)
	{
		dbg_msg("video_recorder", "Failed to create formatcontext for recoding video.");
		return;
	}

	m_pFormat = m_pFormatContext->oformat;



	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if (m_pFormat->video_codec != AV_CODEC_ID_NONE)
	{
		add_stream(&m_VideoStream, m_pFormatContext, &m_VideoCodec, m_pFormat->video_codec);
	}
	else
	{
		dbg_msg("video_recorder", "Failed to add VideoStream for recoding video.");
	}

	if (m_HasAudio && m_pFormat->audio_codec != AV_CODEC_ID_NONE)
	{
		add_stream(&m_AudioStream, m_pFormatContext, &m_AudioCodec, m_pFormat->audio_codec);
	}
	else
	{
		dbg_msg("video_recorder", "No audio.");
	}

	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	open_video();

	if (m_HasAudio)
		open_audio();

	// TODO: remove/comment:
	av_dump_format(m_pFormatContext, 0, aWholePath, 1);

	/* open the output file, if needed */
	if (!(m_pFormat->flags & AVFMT_NOFILE))
	{
		int ret = avio_open(&m_pFormatContext->pb, aWholePath, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			char aBuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, aBuf, sizeof(aBuf));
			dbg_msg("video_recorder", "Could not open '%s': %s", aWholePath, aBuf);
			return;
		}
	}


	/* Write the stream header, if any. */
	int ret = avformat_write_header(m_pFormatContext, &m_pOptDict);
	if (ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Error occurred when opening output file: %s", aBuf);
		return;
	}
	m_Recording = true;
	m_Started = true;
	ms_Time = time_get();
}

void CVideo::stop()
{
	m_Recording = false;

	while (m_ProcessingVideoFrame || m_ProcessingAudioFrame)
		thread_sleep(10);

	finish_frames(&m_VideoStream);

	if (m_HasAudio)
		finish_frames(&m_AudioStream);

	av_write_trailer(m_pFormatContext);

	close_stream(&m_VideoStream);

	if (m_HasAudio)
		close_stream(&m_AudioStream);

	if (!(m_pFormat->flags & AVFMT_NOFILE))
		avio_closep(&m_pFormatContext->pb);

	if (m_pFormatContext)
		avformat_free_context(m_pFormatContext);

	if (m_pRGB)
		free(m_pRGB);

	if (m_pPixels)
		free(m_pPixels);
}

void CVideo::nextVideoFrame_thread()
{
	if (m_NextFrame)
	{
		// #ifdef CONF_PLATFORM_MACOSX
		// 	CAutoreleasePool AutoreleasePool;
		// #endif

		m_VideoStream.frame->pts = m_VideoStream.enc->frame_number;
		dbg_msg("video_recorder", "vframe: %d", m_VideoStream.enc->frame_number);

		read_rgb_from_gl();
		fill_video_frame();
		write_frame(&m_VideoStream);

		m_ProcessingVideoFrame = false;
		m_NextFrame = false;
		// sync_barrier();
		// m_Semaphore.signal();
	}
}

void CVideo::nextVideoFrame()
{
	if (m_Recording)
	{
		// #ifdef CONF_PLATFORM_MACOSX
		// 	CAutoreleasePool AutoreleasePool;
		// #endif

		dbg_msg("video_recorder", "called");

		ms_Time += ms_TickTime;
		ms_LocalTime = (ms_Time-ms_LocalStartTime)/(float)time_freq();
		m_ProcessingVideoFrame = true;
		m_NextFrame = true;

		// m_pGraphics->KickCommandBuffer();
		//thread_sleep(500);

		// m_Semaphore.wait();
	}
}

void CVideo::nextAudioFrame(short* pData)
{
	if (m_Recording && m_HasAudio)
	{
		m_ProcessingAudioFrame = true;
		m_AudioStream.frame->pts = m_AudioStream.enc->frame_number;
		// dbg_msg("video_recorder", "aframe: %d", m_AudioStream.enc->frame_number);

		// memcpy(m_AudioStream.tmp_frame->data[0], pData, sizeof(int16_t) * m_SndBufferSize * 2);
		//
		// for (int i = 0; i < m_SndBufferSize; i++)
		// {
		// 	dbg_msg("video_recorder", "test: %d %d", ((int16_t*)pData)[i*2], ((int16_t*)pData)[i*2 + 1]);
		// }

		// int dst_nb_samples;

		// dst_nb_samples = av_rescale_rnd(
		// 	swr_get_delay(
		// 		m_AudioStream.swr_ctx,
		// 		m_AudioStream.enc->sample_rate
		// 	) + m_AudioStream.tmp_frame->nb_samples,
		//
		// 	m_AudioStream.enc->sample_rate,
		// 	m_AudioStream.enc->sample_rate, AV_ROUND_UP
		// );

		// dbg_msg("video_recorder", "dst_nb_samples: %d", dst_nb_samples);

		av_samples_fill_arrays(
			(uint8_t**)m_AudioStream.tmp_frame->data,
			0, // pointer to linesize (int*)
			(const uint8_t*)pData,
			2, // channels
			m_AudioStream.tmp_frame->nb_samples,
			AV_SAMPLE_FMT_S16P,
			0 // align
		);


		int ret = av_frame_make_writable(m_AudioStream.frame);
		if (ret < 0)
			exit(1);

		/* convert to destination format */
		ret = swr_convert(
			m_AudioStream.swr_ctx,
			m_AudioStream.frame->data,
			m_AudioStream.frame->nb_samples,
			(const uint8_t **)m_AudioStream.tmp_frame->data,
			m_AudioStream.tmp_frame->nb_samples
		);

		if (ret < 0)
		{
			dbg_msg("video_recorder", "Error while converting");
			exit(1);
		}

		// frame = ost->frame;
		//
		// m_AudioStream.frame->pts = av_rescale_q(m_AudioStream.samples_count, (AVRational){1, m_AudioStream.enc->sample_rate}, m_AudioStream.enc->time_base);
		// m_AudioStream.samples_count += dst_nb_samples;

		// dbg_msg("video_recorder", "prewrite----");
		write_frame(&m_AudioStream);

		m_ProcessingAudioFrame = false;
	}
}

void CVideo::fill_audio_frame()
{
	;
}

void CVideo::fill_video_frame()
{
	const int in_linesize[1] = { 3 * m_VideoStream.enc->width };
	if (!m_VideoStream.sws_ctx)
	{
		m_VideoStream.sws_ctx = sws_getCachedContext(
			m_VideoStream.sws_ctx,
			m_VideoStream.enc->width, m_VideoStream.enc->height, AV_PIX_FMT_RGB24,
			m_VideoStream.enc->width, m_VideoStream.enc->height, AV_PIX_FMT_YUV420P,
			0, 0, 0, 0
		);
	}
	sws_scale(m_VideoStream.sws_ctx, (const uint8_t * const *)&m_pRGB, in_linesize, 0,
			m_VideoStream.enc->height, m_VideoStream.frame->data, m_VideoStream.frame->linesize);
}

void CVideo::read_rgb_from_gl()
{
	size_t i, j, k, cur_gl, cur_rgb, nvals;
	const size_t format_nchannels = 3;
	nvals = format_nchannels * m_Width * m_Height;
	m_pPixels = (uint8_t *)realloc(m_pPixels, nvals * sizeof(GLubyte));
	m_pRGB = (uint8_t *)realloc(m_pRGB, nvals * sizeof(uint8_t));
	/* Get RGBA to align to 32 bits instead of just 24 for RGB. May be faster for FFmpeg. */
	glReadPixels(0, 0, m_Width, m_Height, GL_RGB, GL_UNSIGNED_BYTE, m_pPixels);
	for (i = 0; i < m_Height; i++)
	{
		for (j = 0; j < m_Width; j++)
		{
			cur_gl  = format_nchannels * (m_Width * (m_Height - i - 1) + j);
			cur_rgb = format_nchannels * (m_Width * i + j);
			for (k = 0; k < format_nchannels; k++)
				m_pRGB[cur_rgb + k] = m_pPixels[cur_gl + k];
		}
	}
}


AVFrame* CVideo::alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame* picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		dbg_msg("video_recorder", "Could not allocate frame data.");
		exit(1);
	}

	return picture;
}


AVFrame* CVideo::alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		dbg_msg("video_recorder", "Error allocating an audio frame");
		exit(1);
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			dbg_msg("video_recorder", "Error allocating an audio buffer");
			exit(1);
		}
	}

	return frame;
}


void CVideo::open_video()
{
	int ret;
	AVCodecContext* c = m_VideoStream.enc;
	AVDictionary* opt = 0;

	av_dict_copy(&opt, m_pOptDict, 0);

	/* open the codec */
	ret = avcodec_open2(c, m_VideoCodec, &opt);
	av_dict_free(&opt);
	if (ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Could not open video codec: %s", aBuf);
		exit(1);
	}

	/* allocate and init a re-usable frame */
	m_VideoStream.frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!m_VideoStream.frame)
	{
		dbg_msg("video_recorder", "Could not allocate video frame");
		exit(1);
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	m_VideoStream.tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P)
	{
		m_VideoStream.tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!m_VideoStream.tmp_frame)
		{
			dbg_msg("video_recorder", "Could not allocate temporary picture");
			exit(1);
		}
	}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(m_VideoStream.st->codecpar, c);
	if (ret < 0)
	{
		dbg_msg("video_recorder", "Could not copy the stream parameters");
		exit(1);
	}
}


void CVideo::open_audio()
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;

	c = m_AudioStream.enc;

	/* open it */
	av_dict_copy(&opt, m_pOptDict, 0);
	ret = avcodec_open2(c, m_AudioCodec, &opt);
	av_dict_free(&opt);
	if (ret < 0)
	{
		char aBuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, aBuf, sizeof(aBuf));
		dbg_msg("video_recorder", "Could not open audio codec: %s", aBuf);
		exit(1);
	}

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	m_AudioStream.frame	 = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);

	m_AudioStream.tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16P, AV_CH_LAYOUT_STEREO, g_Config.m_SndRate, m_SndBufferSize);

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(m_AudioStream.st->codecpar, c);
	if (ret < 0) {
		dbg_msg("video_recorder", "Could not copy the stream parameters");
		exit(1);
	}

	/* create resampler context */
		m_AudioStream.swr_ctx = swr_alloc();
		if (!m_AudioStream.swr_ctx) {
			dbg_msg("video_recorder", "Could not allocate resampler context");
			exit(1);
		}

		/* set options */
		av_opt_set_int	   (m_AudioStream.swr_ctx, "in_channel_count",   2,	   0);
		av_opt_set_int	   (m_AudioStream.swr_ctx, "in_sample_rate",	 g_Config.m_SndRate,	0);
		av_opt_set_sample_fmt(m_AudioStream.swr_ctx, "in_sample_fmt",	  AV_SAMPLE_FMT_S16P, 0);
		av_opt_set_int	   (m_AudioStream.swr_ctx, "out_channel_count",  c->channels,	   0);
		av_opt_set_int	   (m_AudioStream.swr_ctx, "out_sample_rate",	c->sample_rate,	0);
		av_opt_set_sample_fmt(m_AudioStream.swr_ctx, "out_sample_fmt",	 c->sample_fmt,	 0);

		/* initialize the resampling context */
		if ((ret = swr_init(m_AudioStream.swr_ctx)) < 0) {
			dbg_msg("video_recorder", "Failed to initialize the resampling context");
			exit(1);
		}
}




/* Add an output stream. */
void CVideo::add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id)
{
	AVCodecContext *c;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec))
	{
		dbg_msg("video_recorder", "Could not find encoder for '%s'",
				avcodec_get_name(codec_id));
		exit(1);
	}

	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st)
	{
		dbg_msg("video_recorder", "Could not allocate stream");
		exit(1);
	}
	ost->st->id = oc->nb_streams-1;
	c = avcodec_alloc_context3(*codec);
	if (!c)
	{
		dbg_msg("video_recorder", "Could not alloc an encoding context");
		exit(1);
	}
	ost->enc = c;

	switch ((*codec)->type)
	{
		case AVMEDIA_TYPE_AUDIO:

			// m_MixingRate = g_Config.m_SndRate;
			//
			// // Set 16-bit stereo audio at 22Khz
			// Format.freq = g_Config.m_SndRate; // ignore_convention
			// Format.format = AUDIO_S16; // ignore_convention
			// Format.channels = 2; // ignore_convention
			// Format.samples = g_Config.m_SndBufferSize; // ignore_convention

			c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
			c->bit_rate = g_Config.m_SndRate * 2 * 16;
			c->frame_size = m_SndBufferSize;
			c->sample_rate = g_Config.m_SndRate;
			if ((*codec)->supported_samplerates)
			{
				c->sample_rate = (*codec)->supported_samplerates[0];
				for (int i = 0; (*codec)->supported_samplerates[i]; i++)
				{
					if ((*codec)->supported_samplerates[i] == g_Config.m_SndRate)
						c->sample_rate = g_Config.m_SndRate;
				}
			}
			c->channels = 2;
			c->channel_layout = AV_CH_LAYOUT_STEREO;

			ost->st->time_base = (AVRational){ 1, c->sample_rate };
			break;

		case AVMEDIA_TYPE_VIDEO:
			c->codec_id = codec_id;

			c->bit_rate = 400000;
			/* Resolution must be a multiple of two. */
			c->width = m_Width;
			c->height = m_Height;
			/* timebase: This is the fundamental unit of time (in seconds) in terms
			 * of which frame timestamps are represented. For fixed-fps content,
			 * timebase should be 1/framerate and timestamp increments should be
			 * identical to 1. */
			ost->st->time_base = (AVRational){ 1, m_FPS };
			c->time_base = ost->st->time_base;

			c->gop_size = 12; /* emit one intra frame every twelve frames at most */
			c->pix_fmt = STREAM_PIX_FMT;
			if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
			{
				/* just for testing, we also add B-frames */
				c->max_b_frames = 2;
			}
			if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
			{
				/* Needed to avoid using macroblocks in which some coeffs overflow.
				 * This does not happen with normal video, it just happens here as
				 * the motion of the chroma plane does not match the luma plane. */
				c->mb_decision = 2;
			}
			if (codec_id == AV_CODEC_ID_H264)
			{
				av_opt_set(c->priv_data, "preset", "slow", 0);
				av_opt_set(c->priv_data, "crf", "22", 0);
			}
		break;

		default:
			break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}



void CVideo::write_frame(OutputStream* pStream)
{

	int ret_send, ret_recv = 0;

	AVPacket Packet = { 0 };

	av_init_packet(&Packet);
	Packet.data = 0;
	Packet.size = 0;

	ret_send = avcodec_send_frame(pStream->enc, pStream->frame);
	do
	{
		ret_recv = avcodec_receive_packet(pStream->enc, &Packet);
		if (!ret_recv)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(&Packet, pStream->enc->time_base, pStream->st->time_base);
			Packet.stream_index = pStream->st->index;

			if (int ret = av_interleaved_write_frame(m_pFormatContext, &Packet))
			{
				char aBuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, aBuf, sizeof(aBuf));
				dbg_msg("video_recorder", "Error while writing video frame: %s", aBuf);
			}
		}
		else
			break;
	} while (true);

	if (ret_recv && ret_recv != AVERROR(EAGAIN))
	{
		dbg_msg("video_recorder", "Error encoding frame, error: %d", ret_recv);
	}
}

void CVideo::finish_frames(OutputStream* pStream)
{
	dbg_msg("video_recorder", "------------");
	int ret_send, ret_recv = 0;

	AVPacket Packet = { 0 };

	av_init_packet(&Packet);
	Packet.data = 0;
	Packet.size = 0;

	ret_send = avcodec_send_frame(pStream->enc, 0);
	do
	{
		ret_recv = avcodec_receive_packet(pStream->enc, &Packet);
		if (!ret_recv)
		{
			/* rescale output packet timestamp values from codec to stream timebase */
			av_packet_rescale_ts(&Packet, pStream->enc->time_base, pStream->st->time_base);
			Packet.stream_index = pStream->st->index;

			if (int ret = av_interleaved_write_frame(m_pFormatContext, &Packet))
			{
				char aBuf[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, aBuf, sizeof(aBuf));
				dbg_msg("video_recorder", "Error while writing video frame: %s", aBuf);
			}
		}
		else
			break;
	} while (true);

	if (ret_recv && ret_recv != AVERROR_EOF)
	{
		dbg_msg("video_recorder", "failed to finish recoding, error: %d", ret_recv);
	}
}

void CVideo::close_stream(OutputStream* ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

#endif
