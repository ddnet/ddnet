#ifndef ENGINE_CLIENT_VIDEO_H
#define ENGINE_CLIENT_VIDEO_H

#include <base/lock.h>

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

class IGraphics;
class ISound;
class IStorage;

extern CLock g_WriteLock;

// a wrapper around a single output AVStream
class COutputStream
{
public:
	AVStream *m_pStream = nullptr;
	AVCodecContext *m_pCodecContext = nullptr;

	/* pts of the next frame that will be generated */
	int64_t m_SamplesCount = 0;
	int64_t m_SamplesFrameCount = 0;

	std::vector<AVFrame *> m_vpFrames;
	std::vector<AVFrame *> m_vpTmpFrames;

	std::vector<struct SwsContext *> m_vpSwsContexts;
	std::vector<struct SwrContext *> m_vpSwrContexts;
};

class CVideo : public IVideo
{
public:
	CVideo(IGraphics *pGraphics, ISound *pSound, IStorage *pStorage, int Width, int Height, const char *pName);
	~CVideo();

	bool Start() override REQUIRES(!g_WriteLock);
	void Stop() override;
	void Pause(bool Pause) override;
	bool IsRecording() override { return m_Recording; }

	void NextVideoFrame() override;
	void NextVideoFrameThread() override;

	void NextAudioFrame(ISoundMixFunc Mix) override;
	void NextAudioFrameTimeline(ISoundMixFunc Mix) override;

	static IVideo *Current() { return IVideo::ms_pCurrentVideo; }

	static void Init();

private:
	void RunVideoThread(size_t ParentThreadIndex, size_t ThreadIndex) REQUIRES(!g_WriteLock);
	void FillVideoFrame(size_t ThreadIndex) REQUIRES(!g_WriteLock);
	void UpdateVideoBufferFromGraphics(size_t ThreadIndex);

	void RunAudioThread(size_t ParentThreadIndex, size_t ThreadIndex) REQUIRES(!g_WriteLock);
	void FillAudioFrame(size_t ThreadIndex);

	bool OpenVideo();
	bool OpenAudio();
	AVFrame *AllocPicture(enum AVPixelFormat PixFmt, int Width, int Height);
	AVFrame *AllocAudioFrame(enum AVSampleFormat SampleFmt, uint64_t ChannelLayout, int SampleRate, int NbSamples);

	void WriteFrame(COutputStream *pStream, size_t ThreadIndex) REQUIRES(g_WriteLock);
	void FinishFrames(COutputStream *pStream);
	void CloseStream(COutputStream *pStream);

	bool AddStream(COutputStream *pStream, AVFormatContext *pFormatContext, const AVCodec **ppCodec, enum AVCodecID CodecId) const;

	IGraphics *m_pGraphics;
	IStorage *m_pStorage;
	ISound *m_pSound;

	int m_Width;
	int m_Height;
	char m_aName[256];
	uint64_t m_VideoFrameIndex = 0;
	uint64_t m_AudioFrameIndex = 0;

	int m_FPS;

	bool m_Started;
	bool m_Stopped;
	bool m_Recording;

	size_t m_VideoThreads = 2;
	size_t m_CurVideoThreadIndex = 0;
	size_t m_AudioThreads = 2;
	size_t m_CurAudioThreadIndex = 0;

	class CVideoRecorderThread
	{
	public:
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

	std::vector<std::unique_ptr<CVideoRecorderThread>> m_vpVideoThreads;

	class CAudioRecorderThread
	{
	public:
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

	std::vector<std::unique_ptr<CAudioRecorderThread>> m_vpAudioThreads;

	std::atomic<int32_t> m_ProcessingVideoFrame;
	std::atomic<int32_t> m_ProcessingAudioFrame;

	bool m_HasAudio;

	class CVideoBuffer
	{
	public:
		std::vector<uint8_t> m_vBuffer;
	};
	std::vector<CVideoBuffer> m_vVideoBuffers;
	class CAudioBuffer
	{
	public:
		int16_t m_aBuffer[4096];
	};
	std::vector<CAudioBuffer> m_vAudioBuffers;

	COutputStream m_VideoStream;
	COutputStream m_AudioStream;

	const AVCodec *m_pVideoCodec;
	const AVCodec *m_pAudioCodec;

	AVDictionary *m_pOptDict;

	AVFormatContext *m_pFormatContext;
	const AVOutputFormat *m_pFormat;
};

#endif
