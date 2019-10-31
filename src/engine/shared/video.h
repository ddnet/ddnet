#ifndef ENGINE_SHARED_VIDEO_H
#define ENGINE_SHARED_VIDEO_H

#include <base/system.h>

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

	static int64 time() { return ms_Time; }
	static float LocalTime() { return ms_LocalTime; }
	static void SetLocalStartTime(int64 LocalStartTime) { ms_LocalStartTime = LocalStartTime; }
	static void SetFPS(int fps) { ms_TickTime = time_freq() / fps; }

	void SetBreak(double Speed) { m_Break += 4.0/Speed; } // I think this 4 is related to `Len/2/2` in `Mix` function of /expand/teeworlds/demo/video_3/src/engine/client/sound.cpp
	double GetBreak() { return m_Break; }

protected:
	static IVideo* ms_pCurrentVideo;
	static int64 ms_Time;
	static int64 ms_LocalStartTime;
	static float ms_LocalTime;
	static int64 ms_TickTime;
	double m_Break;
};


#endif
