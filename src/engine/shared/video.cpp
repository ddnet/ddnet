#if defined(CONF_VIDEORECORDER)

#include <engine/shared/config.h>

#include "video.h"

IVideo* IVideo::ms_pCurrentVideo = 0;

int64 IVideo::ms_Time = 0;
float IVideo::ms_LocalTime = 0;
int64 IVideo::ms_LocalStartTime = 0;
int64 IVideo::ms_TickTime = 0;

#endif
