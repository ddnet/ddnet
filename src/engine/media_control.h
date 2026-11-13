#ifndef ENGINE_MEDIA_CONTROL_H
#define ENGINE_MEDIA_CONTROL_H

#include "kernel.h"

#include <chrono>
#include <cstdint>

class CImageInfo;

/**
 * Snapshot of the media that is currently playing on the system
 * (e.g. in a music player or a browser), as reported by the OS.
 */
class CMediaState
{
public:
	/**
	 * Whether there is any media session that can be shown and controlled.
	 */
	bool m_Available = false;
	bool m_Playing = false;

	char m_aTitle[128] = "";
	char m_aArtist[128] = "";
	/**
	 * Name of the application that is playing the media.
	 */
	char m_aSource[64] = "";

	/**
	 * Playback position at the time of @link m_PositionTimestamp @endlink, or negative if unknown.
	 */
	int64_t m_PositionMs = -1;
	/**
	 * Total duration of the media, or negative if unknown.
	 */
	int64_t m_DurationMs = -1;
	/**
	 * Value of `time_get_nanoseconds` when @link m_PositionMs @endlink was sampled.
	 */
	std::chrono::nanoseconds m_PositionTimestamp{0};

	bool m_CanPlayPause = false;
	bool m_CanNext = false;
	bool m_CanPrevious = false;

	/**
	 * Changes whenever the artwork of the media changes, zero if there is no artwork.
	 * The artwork can be retrieved with @link IMediaControl::CopyArtwork @endlink.
	 */
	int m_ArtworkVersion = 0;
};

/**
 * Reads and controls the media that is playing on the system.
 *
 * All functions are cheap and non-blocking, so they can be called from the render thread.
 * Implementations must do any blocking OS communication on their own thread.
 */
class IMediaControl : public IInterface
{
	MACRO_INTERFACE("mediacontrol")
public:
	virtual void Init() = 0;
	void Shutdown() override = 0;

	virtual CMediaState State() = 0;
	/**
	 * Copies the artwork of the current media as an RGBA image.
	 *
	 * @param Image The image to copy the artwork to, must not be allocated.
	 *
	 * @return `true` if there is an artwork and it was copied, `false` otherwise.
	 */
	virtual bool CopyArtwork(CImageInfo &Image) = 0;

	virtual void PlayPause() = 0;
	virtual void Next() = 0;
	virtual void Previous() = 0;
};

IMediaControl *CreateMediaControl();

#endif // ENGINE_MEDIA_CONTROL_H
