#include "media_control.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/image.h>
#include <engine/shared/config.h>

#include <iterator>

class CMockTrack
{
public:
	const char *m_pTitle;
	const char *m_pArtist;
	int64_t m_DurationMs;
	bool m_HasArtwork;
};

static const CMockTrack MOCK_TRACKS[] = {
	{"Never Gonna Give You Up", "Rick Astley", 213000, true},
	{"A very long song title that does not fit into the media card", "An artist with a very long name as well", 95000, true},
	{"Short", "", 20000, false},
};

int64_t CMediaControlNull::MockPosition() const
{
	const int64_t Duration = MOCK_TRACKS[m_MockTrack].m_DurationMs;
	if(!m_MockPlaying)
		return m_MockPositionMs;
	const int64_t Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(time_get_nanoseconds() - m_MockPositionTimestamp).count();
	return (m_MockPositionMs + Elapsed) % Duration;
}

void CMediaControlNull::MockSetTrack(int Track)
{
	const int NumTracks = std::size(MOCK_TRACKS);
	m_MockTrack = ((Track % NumTracks) + NumTracks) % NumTracks;
	m_MockPositionMs = 0;
	m_MockPositionTimestamp = time_get_nanoseconds();
}

void CMediaControlNull::Init()
{
	m_MockPositionTimestamp = time_get_nanoseconds();
}

void CMediaControlNull::Shutdown()
{
}

CMediaState CMediaControlNull::State()
{
	CMediaState State;
	if(!g_Config.m_DbgMediaMock)
		return State;

	const CMockTrack &Track = MOCK_TRACKS[m_MockTrack];
	State.m_Available = true;
	State.m_Playing = m_MockPlaying;
	str_copy(State.m_aTitle, Track.m_pTitle);
	str_copy(State.m_aArtist, Track.m_pArtist);
	str_copy(State.m_aSource, "Mock player");
	State.m_PositionMs = MockPosition();
	State.m_DurationMs = Track.m_DurationMs;
	State.m_PositionTimestamp = time_get_nanoseconds();
	State.m_CanPlayPause = true;
	State.m_CanNext = true;
	State.m_CanPrevious = m_MockTrack > 0;
	State.m_ArtworkVersion = Track.m_HasArtwork ? m_MockTrack + 1 : 0;
	return State;
}

bool CMediaControlNull::CopyArtwork(CImageInfo &Image)
{
	if(!g_Config.m_DbgMediaMock || !MOCK_TRACKS[m_MockTrack].m_HasArtwork)
		return false;

	// simple gradient that is different for each track and not square, to test the cropping
	Image.m_Width = 96;
	Image.m_Height = 64;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	for(size_t y = 0; y < Image.m_Height; ++y)
	{
		for(size_t x = 0; x < Image.m_Width; ++x)
		{
			uint8_t *pPixel = &Image.m_pData[(y * Image.m_Width + x) * 4];
			pPixel[0] = x * 255 / (Image.m_Width - 1);
			pPixel[1] = y * 255 / (Image.m_Height - 1);
			pPixel[2] = m_MockTrack % 2 == 0 ? 255 : 0;
			pPixel[3] = 255;
		}
	}
	return true;
}

void CMediaControlNull::PlayPause()
{
	m_MockPositionMs = MockPosition();
	m_MockPositionTimestamp = time_get_nanoseconds();
	m_MockPlaying = !m_MockPlaying;
}

void CMediaControlNull::Next()
{
	MockSetTrack(m_MockTrack + 1);
}

void CMediaControlNull::Previous()
{
	MockSetTrack(m_MockTrack - 1);
}

IMediaControl *CreateMediaControl()
{
#if defined(MEDIA_CONTROL_WINDOWS)
	return CreateMediaControlWindows();
#else
	return new CMediaControlNull();
#endif
}
