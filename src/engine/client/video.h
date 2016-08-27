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


// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

class CVideo
{
public:
	CVideo(class IStorage* pStorage, class IConsole *pConsole, int width, int height);
	~CVideo();

	void start();
	void stop();

	void nextFrame();

	static CVideo* Current() { return ms_pCurrentVideo; }

	static void Init() { avcodec_register_all(); av_register_all(); }

private:
	void fill_frame();
	void finish_video_frames();
	void write_video_frame();
	void read_rgb_from_gl();

	void open_video();
	void open_audio();
	AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
	AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples);

	bool write_frame(const AVRational *time_base, AVStream *st, AVPacket *pkt);
	void close_stream(OutputStream *ost);

	void add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);

	class IStorage *m_pStorage;
	class IConsole *m_pConsole;

	int m_Width;
	int m_Height;

	bool m_Recording;
	bool m_ProcessingFrame;

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

	static CVideo* ms_pCurrentVideo;
};


#endif
