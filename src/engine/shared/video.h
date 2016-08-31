#ifndef ENGINE_SHARED_VIDEO_H
#define ENGINE_SHARED_VIDEO_H

#include <base/system.h>

#define STREAM_FRAME_RATE 60 /* 60 images/s */

class IVideo
{
public:
	virtual ~IVideo() {};

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void nextVideoFrame() = 0;
	virtual bool frameRendered() = 0;
	virtual void nextVideoFrame_thread() = 0;

	virtual void nextAudioFrame(short* pData) = 0;


	static IVideo* Current() { return ms_pCurrentVideo; }

	static const int64 time() { return ms_Time; }
	static const float LocalTime() { return ms_LocalTime; }
	static void SetLocalStartTime(int64 LocalStartTime) { ms_LocalStartTime = LocalStartTime; }
	static void SetFPS(int fps) { ms_TickTime = time_freq() / fps; }

protected:
	static IVideo* ms_pCurrentVideo;
	static int64 ms_Time;
	static int64 ms_LocalStartTime;
	static float ms_LocalTime;
	static int64 ms_TickTime;
};


#endif
