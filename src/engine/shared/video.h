#ifndef ENGINE_SHARED_VIDEO_H
#define ENGINE_SHARED_VIDEO_H

#include <base/system.h>

class IVideo
{
public:
	virtual ~IVideo(){};

	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Pause(bool Pause) = 0;
	virtual bool IsRecording() = 0;

	virtual void NextVideoFrame() = 0;
	virtual bool FrameRendered() = 0;
	virtual void NextVideoFrameThread() = 0;

	virtual void NextAudioFrame(void (*Mix)(short *pFinalOut, unsigned Frames)) = 0;
	virtual bool AudioFrameRendered() = 0;
	virtual void NextAudioFrameTimeline() = 0;

	static IVideo *Current() { return ms_pCurrentVideo; }

	static int64 Time() { return ms_Time; }
	static float LocalTime() { return ms_LocalTime; }
	static void SetLocalStartTime(int64 LocalStartTime) { ms_LocalStartTime = LocalStartTime; }
	static void SetFPS(int FPS) { ms_TickTime = time_freq() / FPS; }

protected:
	static IVideo *ms_pCurrentVideo;
	static int64 ms_Time;
	static int64 ms_LocalStartTime;
	static float ms_LocalTime;
	static int64 ms_TickTime;
};

#endif
