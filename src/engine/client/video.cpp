#include <engine/storage.h>
#include <engine/console.h>

#include "video.h"

CVideo* CVideo::ms_pCurrentVideo = 0;

CVideo::CVideo(class IStorage* pStorage, class IConsole *pConsole, int width, int height) :
	m_pStorage(pStorage),
	m_pConsole(pConsole)
{
	m_pPixels = 0;
	m_nframes = 0;

	m_pContext = 0;
	m_pSws_context = 0;
	m_pRGB = 0;

	m_Width = width;
	m_Height = height;

	m_Recording = false;
	m_ProcessingFrame = false;

	ms_pCurrentVideo = this;
}

CVideo::~CVideo()
{
	ms_pCurrentVideo = 0;
}

void CVideo::start()
{
	ffmpeg_encoder_start(AV_CODEC_ID_H264, 10, m_Width, m_Height);
	m_Recording = true;
}

void CVideo::stop()
{
	m_Recording = false;
	ffmpeg_encoder_finish();
	if (m_pRGB)
		free(m_pRGB);

	if (m_pPixels)
		free(m_pPixels);
}

void CVideo::nextFrame()
{
	if (m_Recording)
	{
		m_ProcessingFrame = true;
		m_pFrame->pts = m_pContext->frame_number;
		dbg_msg("video_recorder", "frame: %d", m_pContext->frame_number);
		ffmpeg_encoder_glread_rgb();
		ffmpeg_encoder_encode_frame();
		m_nframes++;
		m_ProcessingFrame = false;
	}
}

void CVideo::ffmpeg_encoder_set_frame_yuv_from_rgb(uint8_t *pRGB)
{
	const int in_linesize[1] = { 3 * m_pContext->width };
	m_pSws_context = sws_getCachedContext(
		m_pSws_context,
		m_pContext->width, m_pContext->height, AV_PIX_FMT_RGB24,
		m_pContext->width, m_pContext->height, AV_PIX_FMT_YUV420P,
		0, 0, 0, 0
	);
	sws_scale(m_pSws_context, (const uint8_t * const *)&pRGB, in_linesize, 0,
			m_pContext->height, m_pFrame->data, m_pFrame->linesize);
}

void CVideo::ffmpeg_encoder_start(int codec_id, int fps, int width, int height)
{
	AVCodec *codec;
	int ret;
	avcodec_register_all();
	codec = avcodec_find_encoder((AVCodecID)codec_id);
	if (!codec)
	{
		dbg_msg("video_recorder", "Codec not found");
		exit(1);
	}
	m_pContext= avcodec_alloc_context3(codec);
	if (!m_pContext)
	{
		dbg_msg("video_recorder", "Could not allocate video codec context");
		exit(1);
	}
	m_pContext->bit_rate = 400000;
	m_pContext->width = width;
	m_pContext->height = height;
	m_pContext->time_base.num = 1;
	m_pContext->time_base.den = fps;
	m_pContext->gop_size = 10;
	m_pContext->max_b_frames = 1;
	m_pContext->pix_fmt = AV_PIX_FMT_YUV420P;
	if (codec_id == AV_CODEC_ID_H264)
	{
		av_opt_set(m_pContext->priv_data, "preset", "slow", 0);
		av_opt_set(m_pContext->priv_data, "crf", "22", 0);
	}
	if (avcodec_open2(m_pContext, codec, NULL) < 0)
	{
		dbg_msg("video_recorder", "Could not open codec");
		exit(1);
	}

	m_File = m_pStorage->OpenFile("tmp.mp4", IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if (!m_File)
	{
		dbg_msg("video_recorder", "Could not open %s", "tmp.mp4");
		exit(1);
	}

	m_pFrame = av_frame_alloc();
	if (!m_pFrame)
	{
		dbg_msg("video_recorder", "Could not allocate video frame");
		exit(1);
	}
	m_pFrame->format = m_pContext->pix_fmt;
	m_pFrame->width  = m_pContext->width;
	m_pFrame->height = m_pContext->height;
	ret = av_image_alloc(m_pFrame->data, m_pFrame->linesize, m_pContext->width, m_pContext->height, m_pContext->pix_fmt, 24);
	if (ret < 0)
	{
		dbg_msg("video_recorder", "Could not allocate raw picture buffer");
		exit(1);
	}
}

void CVideo::ffmpeg_encoder_finish()
{
	while (m_ProcessingFrame)
		thread_sleep(10);


	//uint8_t endcode[] = { 0, 0, 1, 0xb7 };
	int ret_send, ret_recv = 0;

	ret_send = avcodec_send_frame(m_pContext, 0);
	do
	{
		ret_recv = avcodec_receive_packet(m_pContext, &m_Packet);
		if (!ret_recv)
		{
			io_write(m_File, m_Packet.data, m_Packet.size);
			av_packet_unref(&m_Packet);
		}
		else
			break;
	} while (true);

	if (ret_recv && ret_recv != AVERROR_EOF)
	{
		dbg_msg("video_recorder", "failed to finish recoding, error: %d", ret_recv);
	}

	//io_write(m_File, endcode, sizeof(endcode));
	io_close(m_File);
	avcodec_close(m_pContext);
	av_free(m_pContext);
	av_freep(&m_pFrame->data[0]);
	av_frame_free(&m_pFrame);
}

void CVideo::ffmpeg_encoder_encode_frame()
{
	if (!m_Recording)
		return;

	int ret_send, ret_recv = 0;

	ffmpeg_encoder_set_frame_yuv_from_rgb(m_pRGB);
	av_init_packet(&m_Packet);
	m_Packet.data = NULL;
	m_Packet.size = 0;

	ret_send = avcodec_send_frame(m_pContext, m_pFrame);
	do
	{
		ret_recv = avcodec_receive_packet(m_pContext, &m_Packet);
		if (!ret_recv)
		{
			io_write(m_File, m_Packet.data, m_Packet.size);
			av_packet_unref(&m_Packet);
		}
		else
			break;
	} while (true);

	if (ret_recv && ret_recv != AVERROR(EAGAIN))
	{
		dbg_msg("video_recorder", "Error encoding frame, error: %d", ret_recv);
	}
}

void CVideo::ffmpeg_encoder_glread_rgb()
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
