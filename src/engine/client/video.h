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
	#include <libavutil/imgutils.h>
	#include <libavutil/opt.h>
	#include <libswscale/swscale.h>
};

#include <base/system.h>

enum Constants { SCREENSHOT_MAX_FILENAME = 256 };

class CVideo
{
public:
	CVideo(class IStorage* pStorage, class IConsole *pConsole, int width, int height);
	~CVideo();

	void start();
	void stop();

	void nextFrame();

	static CVideo* Current() { return ms_pCurrentVideo; }

private:
	void ffmpeg_encoder_set_frame_yuv_from_rgb(uint8_t* pRGB);
	void ffmpeg_encoder_start(int codec_id, int fps, int width, int height);
	void ffmpeg_encoder_finish();
	void ffmpeg_encoder_encode_frame();
	void ffmpeg_encoder_glread_rgb();

	class IStorage *m_pStorage;
	class IConsole *m_pConsole;

	int m_Width;
	int m_Height;

	bool m_Recording;
	bool m_ProcessingFrame;

	GLubyte* m_pPixels;
	unsigned int m_nframes;

	AVCodecContext* m_pContext;
	AVFrame* m_pFrame;
	AVPacket m_Packet;
	IOHANDLE m_File;
	struct SwsContext* m_pSws_context;
	uint8_t* m_pRGB;

	static CVideo* ms_pCurrentVideo;
};


#endif
