#ifndef ENGINE_CLIENT_VIDEO_H
#define ENGINE_CLIENT_VIDEO_H

#if defined(__ANDROID__)
#define GL_GLEXT_PROTOTYPES
#include <GL/glu.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#define glOrtho glOrthof
#else
#include "SDL_opengl.h"

#if defined(CONF_PLATFORM_MACOSX)
#include "OpenGL/glu.h"
#else
#include "GL/glu.h"
#endif
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
};

#include <base/system.h>

#include <engine/shared/demo.h>
#include <engine/shared/video.h>
#define ALEN 2048

extern LOCK g_WriteLock;

// a wrapper around a single output AVStream
typedef struct OutputStream
{
	AVStream *pSt;
	AVCodecContext *pEnc;

	/* pts of the next frame that will be generated */
	int64 NextPts;
	int SamplesCount;

	AVFrame *pFrame;
	AVFrame *pTmpFrame;

	struct SwsContext *pSwsCtx;
	struct SwrContext *pSwrCtx;
} OutputStream;

class CVideo : public IVideo
{
public:
	CVideo(class CGraphics_Threaded *pGraphics, class IStorage *pStorage, class IConsole *pConsole, int width, int height, const char *name);
	~CVideo();

	virtual void Start();
	virtual void Stop();
	virtual void Pause(bool Pause);
	virtual bool IsRecording() { return m_Recording; }

	virtual void NextVideoFrame();
	virtual void NextVideoFrameThread();
	virtual bool FrameRendered() { return !m_NextFrame; }

	virtual void NextAudioFrame(void (*Mix)(short *pFinalOut, unsigned Frames));
	virtual void NextAudioFrameTimeline();
	virtual bool AudioFrameRendered() { return !m_NextAudioFrame; }

	static IVideo *Current() { return IVideo::ms_pCurrentVideo; }

	static void Init() { av_log_set_level(AV_LOG_DEBUG); }

private:
	void FillVideoFrame();
	void ReadRGBFromGL();

	void FillAudioFrame();

	bool OpenVideo();
	bool OpenAudio();
	AVFrame *AllocPicture(enum AVPixelFormat PixFmt, int Width, int Height);
	AVFrame *AllocAudioFrame(enum AVSampleFormat SampleFmt, uint64 ChannelLayout, int SampleRate, int NbSamples);

	void WriteFrame(OutputStream *pStream) REQUIRES(g_WriteLock);
	void FinishFrames(OutputStream *pStream);
	void CloseStream(OutputStream *pStream);

	bool AddStream(OutputStream *pStream, AVFormatContext *pOC, AVCodec **ppCodec, enum AVCodecID CodecId);

	class CGraphics_Threaded *m_pGraphics;
	class IStorage *m_pStorage;
	class IConsole *m_pConsole;

	int m_Width;
	int m_Height;
	char m_Name[256];
	//FILE *m_dbgfile;
	int m_Vseq;
	short m_aBuffer[ALEN * 2];
	int m_Vframe;

	int m_FPS;

	bool m_Started;
	bool m_Recording;

	bool m_ProcessingVideoFrame;
	bool m_ProcessingAudioFrame;

	bool m_NextFrame;
	bool m_NextAudioFrame;

	bool m_HasAudio;

	GLubyte *m_pPixels;

	OutputStream m_VideoStream;
	OutputStream m_AudioStream;

	AVCodec *m_VideoCodec;
	AVCodec *m_AudioCodec;

	AVDictionary *m_pOptDict;

	AVFormatContext *m_pFormatContext;
	AVOutputFormat *m_pFormat;

	uint8_t *m_pRGB;

	int m_SndBufferSize;
};

#endif
