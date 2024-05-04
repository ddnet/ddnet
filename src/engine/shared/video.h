#ifndef ENGINE_SHARED_VIDEO_H
#define ENGINE_SHARED_VIDEO_H

#include <base/system.h>

#include <functional>

typedef std::function<void(short *pFinalOut, unsigned Frames)> ISoundMixFunc;

class IVideo
{
public:
	virtual ~IVideo(){};

	virtual bool Start() = 0;
	virtual void Stop() = 0;
	virtual void Pause(bool Pause) = 0;
	virtual bool IsRecording() = 0;

	virtual void NextVideoFrame() = 0;
	virtual void NextVideoFrameThread() = 0;

	virtual void NextAudioFrame(ISoundMixFunc Mix) = 0;
	virtual void NextAudioFrameTimeline(ISoundMixFunc Mix) = 0;

	static IVideo *Current() { return ms_pCurrentVideo; }

	static int64_t Time() { return ms_Time; }
	static float LocalTime() { return ms_LocalTime; }
	static void SetLocalStartTime(int64_t LocalStartTime) { ms_LocalStartTime = LocalStartTime; }
	static void SetFPS(int FPS) { ms_TickTime = time_freq() / FPS; }

protected:
	static IVideo *ms_pCurrentVideo;
	static int64_t ms_Time;
	static int64_t ms_LocalStartTime;
	static float ms_LocalTime;
	static int64_t ms_TickTime;
};

#endif
