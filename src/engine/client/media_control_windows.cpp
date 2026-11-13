#include "media_control.h"

#if defined(MEDIA_CONTROL_WINDOWS)

#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/thread.h>
#include <base/time.h>

#include <engine/image.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;

/**
 * Media control implementation using the System Media Transport Controls
 * of Windows 10 (version 1809) and newer, which is also what the media
 * flyout of Windows itself uses.
 *
 * All communication with the system happens on a separate thread, because
 * the functions can block for a noticeable time. The thread only polls the
 * system while the state is being requested.
 */
class CMediaControlWindows : public IMediaControl
{
	enum
	{
		COMMAND_PLAY_PAUSE = 1 << 0,
		COMMAND_NEXT = 1 << 1,
		COMMAND_PREVIOUS = 1 << 2,
	};

	static constexpr auto POLL_INTERVAL = 250ms;
	// Time to give the player to react to a command before polling the state again.
	static constexpr auto COMMAND_REACTION_TIME = 100ms;
	// Players often set the artwork shortly after the other properties, so keep checking it for some time.
	static constexpr auto ARTWORK_CHECK_DURATION = 3s;
	static constexpr uint64_t ARTWORK_MAX_FILE_SIZE = 16 * 1024 * 1024;
	// The artwork is scaled down to this size, as it is only shown very small.
	static constexpr uint32_t ARTWORK_MAX_SIZE = 256;

	void *m_pThread = nullptr;

	std::mutex m_Mutex;
	std::condition_variable m_Condition;
	// The following variables are protected by the mutex.
	CMediaState m_State;
	unsigned m_PendingCommands = 0;
	bool m_StateRequested = false;
	bool m_Shutdown = false;
	std::vector<uint8_t> m_vArtworkPixels; // RGBA
	uint32_t m_ArtworkWidth = 0;
	uint32_t m_ArtworkHeight = 0;

	// The following variables are only used by the thread.
	winrt::hstring m_SourceAppUserModelId;
	std::string m_SourceName;
	winrt::hstring m_ArtworkKey;
	std::chrono::steady_clock::time_point m_ArtworkCheckUntil;
	std::vector<uint8_t> m_vArtworkFile;
	int m_ArtworkVersion = 0;
	int m_ArtworkVersionCounter = 0;
	// Decoded artwork that was not made available to the main thread yet.
	bool m_PendingArtworkChanged = false;
	std::vector<uint8_t> m_vPendingArtworkPixels;
	uint32_t m_PendingArtworkWidth = 0;
	uint32_t m_PendingArtworkHeight = 0;

	void QueueCommand(unsigned Command)
	{
		{
			const std::unique_lock Lock(m_Mutex);
			m_PendingCommands |= Command;
		}
		m_Condition.notify_one();
	}

	/**
	 * Determines the user-facing name of an application from its AppUserModelId.
	 */
	const char *SourceName(const winrt::hstring &AppUserModelId)
	{
		if(AppUserModelId == m_SourceAppUserModelId)
			return m_SourceName.c_str();

		m_SourceAppUserModelId = AppUserModelId;
		m_SourceName.clear();
		try
		{
			// Only supported since Windows 10 version 2004
			const auto AppInfo = winrt::Windows::ApplicationModel::AppInfo::GetFromAppUserModelId(AppUserModelId);
			m_SourceName = winrt::to_string(AppInfo.DisplayInfo().DisplayName());
		}
		catch(const winrt::hresult_error &)
		{
		}
		if(m_SourceName.empty())
		{
			// Unpackaged applications often use the name of the executable as their ID
			m_SourceName = winrt::to_string(AppUserModelId);
			const char *pExtension = str_endswith_nocase(m_SourceName.c_str(), ".exe");
			if(pExtension != nullptr)
				m_SourceName.resize(pExtension - m_SourceName.c_str());
		}
		return m_SourceName.c_str();
	}

	void SetPendingArtwork(std::vector<uint8_t> &&vPixels, uint32_t Width, uint32_t Height)
	{
		m_vPendingArtworkPixels = std::move(vPixels);
		m_PendingArtworkWidth = Width;
		m_PendingArtworkHeight = Height;
		m_PendingArtworkChanged = true;
		m_ArtworkVersion = m_vPendingArtworkPixels.empty() ? 0 : ++m_ArtworkVersionCounter;
	}

	void ClearArtwork()
	{
		m_ArtworkKey.clear();
		m_vArtworkFile.clear();
		if(m_ArtworkVersion != 0)
			SetPendingArtwork({}, 0, 0);
	}

	void UpdateArtwork(const GlobalSystemMediaTransportControlsSessionMediaProperties &Properties, const winrt::hstring &SourceAppUserModelId)
	{
		// Only check the artwork for some time after the media has changed, as reading it is expensive
		const winrt::hstring Key = SourceAppUserModelId + L"\n" + Properties.Title() + L"\n" + Properties.Artist() + L"\n" + Properties.AlbumTitle();
		const auto Now = std::chrono::steady_clock::now();
		if(Key != m_ArtworkKey)
		{
			m_ArtworkKey = Key;
			m_ArtworkCheckUntil = Now + ARTWORK_CHECK_DURATION;
		}
		else if(Now > m_ArtworkCheckUntil)
		{
			return;
		}

		try
		{
			std::vector<uint8_t> vFile;
			IRandomAccessStreamWithContentType Stream(nullptr);
			const auto Thumbnail = Properties.Thumbnail();
			if(Thumbnail)
			{
				Stream = Thumbnail.OpenReadAsync().get();
				const uint64_t Size = Stream.Size();
				if(Size > 0 && Size <= ARTWORK_MAX_FILE_SIZE)
				{
					const DataReader Reader(Stream.GetInputStreamAt(0));
					Reader.LoadAsync(static_cast<uint32_t>(Size)).get();
					vFile.resize(Reader.UnconsumedBufferLength());
					Reader.ReadBytes(vFile);
				}
			}

			// Only decode the artwork when the file has changed
			if(vFile == m_vArtworkFile)
				return;
			m_vArtworkFile = std::move(vFile);
			if(m_vArtworkFile.empty())
			{
				SetPendingArtwork({}, 0, 0);
				return;
			}

			const auto Decoder = BitmapDecoder::CreateAsync(Stream).get();
			const uint32_t Width = Decoder.PixelWidth();
			const uint32_t Height = Decoder.PixelHeight();
			if(Width == 0 || Height == 0)
			{
				SetPendingArtwork({}, 0, 0);
				return;
			}
			const float Scale = std::min(1.0f, ARTWORK_MAX_SIZE / static_cast<float>(std::max(Width, Height)));
			const uint32_t ScaledWidth = std::max<uint32_t>(Width * Scale, 1);
			const uint32_t ScaledHeight = std::max<uint32_t>(Height * Scale, 1);
			const BitmapTransform Transform;
			Transform.ScaledWidth(ScaledWidth);
			Transform.ScaledHeight(ScaledHeight);
			Transform.InterpolationMode(BitmapInterpolationMode::Fant);
			const auto PixelData = Decoder.GetPixelDataAsync(BitmapPixelFormat::Rgba8, BitmapAlphaMode::Straight, Transform, ExifOrientationMode::IgnoreExifOrientation, ColorManagementMode::DoNotColorManage).get();
			const auto Pixels = PixelData.DetachPixelData();
			if(Pixels.size() != static_cast<size_t>(ScaledWidth) * ScaledHeight * 4)
			{
				SetPendingArtwork({}, 0, 0);
				return;
			}
			SetPendingArtwork(std::vector<uint8_t>(Pixels.begin(), Pixels.end()), ScaledWidth, ScaledHeight);
		}
		catch(const winrt::hresult_error &Error)
		{
			log_debug("mediacontrol", "Failed to get media artwork: %s", winrt::to_string(Error.message()).c_str());
			SetPendingArtwork({}, 0, 0);
		}
	}

	CMediaState Poll(const GlobalSystemMediaTransportControlsSessionManager &Manager)
	{
		CMediaState State;
		try
		{
			const auto Session = Manager.GetCurrentSession();
			if(!Session)
			{
				ClearArtwork();
				return State;
			}

			const auto Properties = Session.TryGetMediaPropertiesAsync().get();
			const auto PlaybackInfo = Session.GetPlaybackInfo();
			const auto Controls = PlaybackInfo.Controls();
			const auto Timeline = Session.GetTimelineProperties();

			State.m_Available = true;
			State.m_Playing = PlaybackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
			str_copy(State.m_aTitle, winrt::to_string(Properties.Title()).c_str());
			str_copy(State.m_aArtist, winrt::to_string(Properties.Artist()).c_str());
			str_copy(State.m_aSource, SourceName(Session.SourceAppUserModelId()));
			State.m_CanPlayPause = Controls.IsPlayPauseToggleEnabled() || (State.m_Playing ? Controls.IsPauseEnabled() : Controls.IsPlayEnabled());
			State.m_CanNext = Controls.IsNextEnabled();
			State.m_CanPrevious = Controls.IsPreviousEnabled();

			const auto Duration = Timeline.EndTime() - Timeline.StartTime();
			if(Duration > 0s)
			{
				auto Position = Timeline.Position() - Timeline.StartTime();
				const auto LastUpdated = Timeline.LastUpdatedTime();
				if(State.m_Playing && LastUpdated.time_since_epoch() > 0s)
				{
					// The position is only updated by the player occasionally, so advance it to the current time
					const auto PlaybackRate = PlaybackInfo.PlaybackRate();
					const auto Elapsed = winrt::clock::from_sys(std::chrono::system_clock::now()) - LastUpdated;
					if(Elapsed > 0s)
						Position += std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(Elapsed * (PlaybackRate ? PlaybackRate.Value() : 1.0));
				}
				Position = std::clamp<winrt::Windows::Foundation::TimeSpan>(Position, 0s, Duration);
				State.m_PositionMs = std::chrono::duration_cast<std::chrono::milliseconds>(Position).count();
				State.m_DurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(Duration).count();
				State.m_PositionTimestamp = time_get_nanoseconds();
			}

			UpdateArtwork(Properties, Session.SourceAppUserModelId());
			State.m_ArtworkVersion = m_ArtworkVersion;
		}
		catch(const winrt::hresult_error &Error)
		{
			log_debug("mediacontrol", "Failed to get media state: %s", winrt::to_string(Error.message()).c_str());
			State = CMediaState();
		}
		return State;
	}

	static void ExecuteCommands(const GlobalSystemMediaTransportControlsSessionManager &Manager, unsigned Commands)
	{
		try
		{
			const auto Session = Manager.GetCurrentSession();
			if(!Session)
				return;

			if(Commands & COMMAND_PLAY_PAUSE)
				Session.TryTogglePlayPauseAsync().get();
			if(Commands & COMMAND_NEXT)
				Session.TrySkipNextAsync().get();
			if(Commands & COMMAND_PREVIOUS)
				Session.TrySkipPreviousAsync().get();
		}
		catch(const winrt::hresult_error &Error)
		{
			log_debug("mediacontrol", "Failed to control media: %s", winrt::to_string(Error.message()).c_str());
		}
	}

	void Run()
	{
		GlobalSystemMediaTransportControlsSessionManager Manager(nullptr);
		try
		{
			winrt::init_apartment(winrt::apartment_type::multi_threaded);
			Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
		}
		catch(const winrt::hresult_error &Error)
		{
			// Expected on Windows versions older than Windows 10 version 1809
			log_info("mediacontrol", "System media controls are not available: %s", winrt::to_string(Error.message()).c_str());
			return;
		}

		std::unique_lock Lock(m_Mutex);
		while(true)
		{
			// Only communicate with the system while the state is being used
			m_Condition.wait(Lock, [&] { return m_Shutdown || m_PendingCommands != 0 || m_StateRequested; });
			if(m_Shutdown)
				break;
			const unsigned Commands = std::exchange(m_PendingCommands, 0);
			m_StateRequested = false;

			Lock.unlock();
			if(Commands != 0)
			{
				ExecuteCommands(Manager, Commands);
				std::this_thread::sleep_for(COMMAND_REACTION_TIME);
			}
			const CMediaState State = Poll(Manager);
			Lock.lock();

			m_State = State;
			if(m_PendingArtworkChanged)
			{
				m_vArtworkPixels = std::move(m_vPendingArtworkPixels);
				m_vPendingArtworkPixels.clear();
				m_ArtworkWidth = m_PendingArtworkWidth;
				m_ArtworkHeight = m_PendingArtworkHeight;
				m_PendingArtworkChanged = false;
			}
			m_Condition.wait_for(Lock, POLL_INTERVAL, [&] { return m_Shutdown || m_PendingCommands != 0; });
			if(m_Shutdown)
				break;
		}
	}

	static void ThreadFunc(void *pUser)
	{
		static_cast<CMediaControlWindows *>(pUser)->Run();
	}

public:
	~CMediaControlWindows() override
	{
		CMediaControlWindows::Shutdown();
	}

	void Init() override
	{
		m_pThread = thread_init(ThreadFunc, this, "media control");
	}

	void Shutdown() override
	{
		if(m_pThread == nullptr)
			return;

		{
			const std::unique_lock Lock(m_Mutex);
			m_Shutdown = true;
		}
		m_Condition.notify_one();
		thread_wait(m_pThread);
		m_pThread = nullptr;
	}

	CMediaState State() override
	{
		bool Notify = false;
		CMediaState State;
		{
			const std::unique_lock Lock(m_Mutex);
			Notify = !m_StateRequested;
			m_StateRequested = true;
			State = m_State;
		}
		if(Notify)
			m_Condition.notify_one();
		return State;
	}

	bool CopyArtwork(CImageInfo &Image) override
	{
		const std::unique_lock Lock(m_Mutex);
		if(m_vArtworkPixels.empty())
			return false;

		Image.m_Width = m_ArtworkWidth;
		Image.m_Height = m_ArtworkHeight;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.Allocate();
		mem_copy(Image.m_pData, m_vArtworkPixels.data(), m_vArtworkPixels.size());
		return true;
	}

	void PlayPause() override
	{
		QueueCommand(COMMAND_PLAY_PAUSE);
	}

	void Next() override
	{
		QueueCommand(COMMAND_NEXT);
	}

	void Previous() override
	{
		QueueCommand(COMMAND_PREVIOUS);
	}
};

IMediaControl *CreateMediaControlWindows()
{
	return new CMediaControlWindows();
}

#endif // defined(MEDIA_CONTROL_WINDOWS)
