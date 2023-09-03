#ifndef ENGINE_CLIENT_VIDEO_H
#define ENGINE_CLIENT_VIDEO_H

#include <base/system.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};

#include <engine/shared/video.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#define ALEN 2048

class CGraphics_Threaded;
class ISound;
class IStorage;

extern LOCK g_WriteLock;

// a wrapper around a single output AVStream
struct OutputStream
{
	AVStream *pSt = nullptr;
	AVCodecContext *pEnc = nullptr;

	/* pts of the next frame that will be generated */
	int64_t NextPts = 0;
	int64_t m_SamplesCount = 0;
	int64_t m_SamplesFrameCount = 0;

	std::vector<AVFrame *> m_vpFrames;
	std::vector<AVFrame *> m_vpTmpFrames;

	std::vector<struct SwsContext *> m_vpSwsCtxs;
	std::vector<struct SwrContext *> m_vpSwrCtxs;
};

class CVideo : public IVideo
{
public:
	CVideo(CGraphics_Threaded *pGraphics, ISound *pSound, IStorage *pStorage, int Width, int Height, const char *pName);
	~CVideo();

	void Start() override REQUIRES(!g_WriteLock);
	void Stop() override;
	void Pause(bool Pause) override;
	bool IsRecording() override { return m_Recording; }

	void NextVideoFrame() override;
	void NextVideoFrameThread() override;

	void NextAudioFrame(ISoundMixFunc Mix) override;
	void NextAudioFrameTimeline(ISoundMixFunc Mix) override;

	static IVideo *Current() { return IVideo::ms_pCurrentVideo; }

	static void Init() { av_log_set_level(AV_LOG_DEBUG); }

private:
	void RunVideoThread(size_t ParentThreadIndex, size_t ThreadIndex) REQUIRES(!g_WriteLock);
	void FillVideoFrame(size_t ThreadIndex) REQUIRES(!g_WriteLock);
	void ReadRGBFromGL(size_t ThreadIndex);

	void RunAudioThread(size_t ParentThreadIndex, size_t ThreadIndex) REQUIRES(!g_WriteLock);
	void FillAudioFrame(size_t ThreadIndex);

	bool OpenVideo();
	bool OpenAudio();
	AVFrame *AllocPicture(enum AVPixelFormat PixFmt, int Width, int Height);
	AVFrame *AllocAudioFrame(enum AVSampleFormat SampleFmt, uint64_t ChannelLayout, int SampleRate, int NbSamples);

	void WriteFrame(OutputStream *pStream, size_t ThreadIndex) REQUIRES(g_WriteLock);
	void FinishFrames(OutputStream *pStream);
	void CloseStream(OutputStream *pStream);

	bool AddStream(OutputStream *pStream, AVFormatContext *pOC, const AVCodec **ppCodec, enum AVCodecID CodecId);

	CGraphics_Threaded *m_pGraphics;
	IStorage *m_pStorage;
	ISound *m_pSound;

	int m_Width;
	int m_Height;
	char m_aName[256];
	//FILE *m_dbgfile;
	uint64_t m_VSeq = 0;
	uint64_t m_ASeq = 0;
	uint64_t m_Vframe;

	int m_FPS;

	bool m_Started;
	bool m_Recording;

	size_t m_VideoThreads = 2;
	size_t m_CurVideoThreadIndex = 0;
	size_t m_AudioThreads = 2;
	size_t m_CurAudioThreadIndex = 0;

	struct SVideoRecorderThread
	{
		std::thread m_Thread;
		std::mutex m_Mutex;
		std::condition_variable m_Cond;

		bool m_Started = false;
		bool m_Finished = false;
		bool m_HasVideoFrame = false;

		std::mutex m_VideoFillMutex;
		std::condition_variable m_VideoFillCond;
		uint64_t m_VideoFrameToFill = 0;
	};

	std::vector<std::unique_ptr<SVideoRecorderThread>> m_vVideoThreads;

	struct SAudioRecorderThread
	{
		std::thread m_Thread;
		std::mutex m_Mutex;
		std::condition_variable m_Cond;

		bool m_Started = false;
		bool m_Finished = false;
		bool m_HasAudioFrame = false;

		std::mutex m_AudioFillMutex;
		std::condition_variable m_AudioFillCond;
		uint64_t m_AudioFrameToFill = 0;
		int64_t m_SampleCountStart = 0;
	};

	std::vector<std::unique_ptr<SAudioRecorderThread>> m_vAudioThreads;

	std::atomic<int32_t> m_ProcessingVideoFrame;
	std::atomic<int32_t> m_ProcessingAudioFrame;

	bool m_HasAudio;

	struct SVideoSoundBuffer
	{
		int16_t m_aBuffer[ALEN * 2];
	};
	std::vector<SVideoSoundBuffer> m_vBuffer;
	std::vector<std::vector<uint8_t>> m_vPixelHelper;

	OutputStream m_VideoStream;
	OutputStream m_AudioStream;

	const AVCodec *m_pVideoCodec;
	const AVCodec *m_pAudioCodec;

	AVDictionary *m_pOptDict;

	AVFormatContext *m_pFormatContext;
	const AVOutputFormat *m_pFormat;
};

#endif
