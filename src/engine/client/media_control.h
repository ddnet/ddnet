#ifndef ENGINE_CLIENT_MEDIA_CONTROL_H
#define ENGINE_CLIENT_MEDIA_CONTROL_H

#include <base/detect.h>

#include <engine/media_control.h>

#if defined(CONF_FAMILY_WINDOWS) && defined(_MSC_VER)
// The implementation uses C++/WinRT, which is only supported by MSVC.
#define MEDIA_CONTROL_WINDOWS
IMediaControl *CreateMediaControlWindows();
#endif

/**
 * Fallback implementation for platforms without a media control backend.
 *
 * Reports that nothing is playing, unless `dbg_media_mock` is enabled,
 * in which case it simulates a player so the UI can be tested.
 */
class CMediaControlNull : public IMediaControl
{
	bool m_MockPlaying = true;
	int m_MockTrack = 0;
	int64_t m_MockPositionMs = 0;
	std::chrono::nanoseconds m_MockPositionTimestamp{0};

	int64_t MockPosition() const;
	void MockSetTrack(int Track);

public:
	void Init() override;
	void Shutdown() override;

	CMediaState State() override;
	bool CopyArtwork(CImageInfo &Image) override;

	void PlayPause() override;
	void Next() override;
	void Previous() override;
};

#endif // ENGINE_CLIENT_MEDIA_CONTROL_H
