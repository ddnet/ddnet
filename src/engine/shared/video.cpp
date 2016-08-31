#if defined(CONF_VIDEORECORDER)

#include "video.h"

IVideo* IVideo::ms_pCurrentVideo = 0;

int64 IVideo::ms_Time = 0;
float IVideo::ms_LocalTime = 0;
int64 IVideo::ms_LocalStartTime = 0;
int64 IVideo::ms_TickTime = time_freq() / STREAM_FRAME_RATE;

#endif
