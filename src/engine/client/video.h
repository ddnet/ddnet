#ifndef ENGINE_CLIENT_VIDEO_H
#define ENGINE_CLIENT_VIDEO_H

#if defined(__ANDROID__)
	#define GL_GLEXT_PROTOTYPES
	#include <GLES/gl.h>
	#include <GLES/glext.h>
	#include <GL/glu.h>
	#define glOrtho glOrthof
#else
	#include "SDL_opengl.h"

	#if defined(CONF_PLATFORM_MACOSX)
		#include "OpenGL/glu.h"
	#else
		#include "GL/glu.h"
	#endif
#endif


extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/opt.h>
	#include <libswscale/swscale.h>
	#include <libswresample/swresample.h>
};

#include <base/system.h>

#include <engine/shared/video.h>


// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;
	AVCodecContext *enc;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
} OutputStream;

class CVideo : public IVideo
{
public:
	CVideo(class CGraphics_Threaded* pGraphics, class IStorage* pStorage, class IConsole *pConsole, int width, int height);
	~CVideo();

	virtual void start();
	virtual void stop();

	virtual void nextVideoFrame();
	virtual void nextVideoFrame_thread();
	virtual bool frameRendered() { return !m_NextFrame; };

	virtual void nextAudioFrame(short* pData);

	static IVideo* Current() { return IVideo::ms_pCurrentVideo; }

	static void Init() { av_log_set_level(AV_LOG_DEBUG); avcodec_register_all(); av_register_all(); }

private:
	void fill_video_frame();
	void read_rgb_from_gl();

	void fill_audio_frame();

	void open_video();
	void open_audio();
	AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
	AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples);

	void write_frame(OutputStream* pStream);
	void finish_frames(OutputStream* pStream);
	void close_stream(OutputStream *ost);

	void add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);

	class CGraphics_Threaded* m_pGraphics;
	class IStorage* m_pStorage;
	class IConsole* m_pConsole;

	int m_Width;
	int m_Height;

	int m_FPS;

	bool m_Started;
	bool m_Recording;

	bool m_ProcessingVideoFrame;
	bool m_ProcessingAudioFrame;

	bool m_NextFrame;

	bool m_HasAudio;

	GLubyte* m_pPixels;

	OutputStream m_VideoStream;
	OutputStream m_AudioStream;

	AVCodec* m_VideoCodec;
	AVCodec* m_AudioCodec;

	AVDictionary* m_pOptDict;

	AVFormatContext* m_pFormatContext;
	AVOutputFormat* m_pFormat;

	uint8_t* m_pRGB;

	int m_SndBufferSize;
};


#endif
