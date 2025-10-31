/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_DEMO_H
#define ENGINE_SHARED_DEMO_H

#include "snapshot.h"

#include <base/hash.h>

#include <engine/demo.h>
#include <engine/shared/protocol.h>

#include <functional>
#include <vector>

typedef std::function<void()> TUpdateIntraTimesFunc;

class CDemoRecorder : public IDemoRecorder
{
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	IOHANDLE m_File;
	char m_aCurrentFilename[IO_MAX_PATH_LENGTH];
	int m_LastTickMarker;
	int m_LastKeyFrame;
	int m_FirstTick;

	unsigned char m_aLastSnapshotData[CSnapshot::MAX_SIZE];
	class CSnapshotDelta *m_pSnapshotDelta;

	int m_NumTimelineMarkers;
	int m_aTimelineMarkers[MAX_TIMELINE_MARKERS];

	bool m_NoMapData;

	DEMOFUNC_FILTER m_pfnFilter;
	void *m_pUser;

	void WriteTickMarker(int Tick, bool Keyframe);
	void Write(int Type, const void *pData, int Size);

public:
	CDemoRecorder(class CSnapshotDelta *pSnapshotDelta, bool NoMapData = false);
	CDemoRecorder() = default;
	~CDemoRecorder() override;

	int Start(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, const char *pNetversion, const char *pMap, const SHA256_DIGEST &Sha256, unsigned MapCrc, const char *pType, unsigned MapSize, unsigned char *pMapData, IOHANDLE MapFile, DEMOFUNC_FILTER pfnFilter, void *pUser);
	int Stop(IDemoRecorder::EStopMode Mode, const char *pTargetFilename = "") override;

	void AddDemoMarker();
	void AddDemoMarker(int Tick);

	void RecordSnapshot(int Tick, const void *pData, int Size);
	void RecordMessage(const void *pData, int Size);

	bool IsRecording() const override { return m_File != nullptr; }
	const char *CurrentFilename() const override { return m_aCurrentFilename; }

	int Length() const override { return (m_LastTickMarker - m_FirstTick) / SERVER_TICK_SPEED; }
};

class CDemoPlayer : public IDemoPlayer
{
public:
	class IListener
	{
	public:
		virtual ~IListener() = default;
		virtual void OnDemoPlayerSnapshot(void *pData, int Size) = 0;
		virtual void OnDemoPlayerMessage(void *pData, int Size) = 0;
	};

	class CPlaybackInfo
	{
	public:
		CDemoHeader m_Header;
		CTimelineMarkers m_TimelineMarkers;

		IDemoPlayer::CInfo m_Info;

		int64_t m_LastUpdate;
		int64_t m_LastScan;
		int64_t m_CurrentTime;

		int m_NextTick;
		int m_PreviousTick;

		float m_IntraTick;
		float m_IntraTickSincePrev;
		float m_TickTime;

		bool m_LiveStateUpdating;
		int m_LiveStateFailedCount;
		int m_LiveStateUnchangedCount;
	};

private:
	IListener *m_pListener;

	TUpdateIntraTimesFunc m_UpdateIntraTimesFunc;

	// Playback
	class CKeyFrame
	{
	public:
		int64_t m_Filepos;
		int m_Tick;

		CKeyFrame(int64_t Filepos, int Tick) :
			m_Filepos(Filepos), m_Tick(Tick)
		{
		}
	};

	class IConsole *m_pConsole;
	IOHANDLE m_File;
	int64_t m_MapOffset;
	char m_aFilename[IO_MAX_PATH_LENGTH];
	char m_aErrorMessage[256];
	std::vector<CKeyFrame> m_vKeyFrames;
	CMapInfo m_MapInfo;
	int m_SpeedIndex;

	CPlaybackInfo m_Info;
	unsigned char m_aCompressedSnapshotData[CSnapshot::MAX_SIZE];
	unsigned char m_aDecompressedSnapshotData[CSnapshot::MAX_SIZE];

	// Depending on the chunk header
	// this is either a full CSnapshot or a CSnapshotDelta.
	unsigned char m_aChunkData[CSnapshot::MAX_SIZE];
	// Storage for the full snapshot
	// where the delta gets unpacked into.
	unsigned char m_aSnapshot[CSnapshot::MAX_SIZE];
	unsigned char m_aLastSnapshotData[CSnapshot::MAX_SIZE];
	int m_LastSnapshotDataSize;
	class CSnapshotDelta *m_pSnapshotDelta;

	bool m_UseVideo;
#if defined(CONF_VIDEORECORDER)
	bool m_WasRecording = false;
#endif

	enum EReadChunkHeaderResult
	{
		CHUNKHEADER_SUCCESS,
		CHUNKHEADER_ERROR,
		CHUNKHEADER_EOF,
	};
	EReadChunkHeaderResult ReadChunkHeader(int *pType, int *pSize, int *pTick);
	void DoTick();
	enum class EScanFileResult
	{
		SUCCESS,
		ERROR_RECOVERABLE,
		ERROR_UNRECOVERABLE,
	};
	EScanFileResult ScanFile();
	void UpdateTimes();

	int64_t Time();
	bool m_Sixup;

public:
	CDemoPlayer(class CSnapshotDelta *pSnapshotDelta, bool UseVideo);
	CDemoPlayer(class CSnapshotDelta *pSnapshotDelta, bool UseVideo, TUpdateIntraTimesFunc &&UpdateIntraTimesFunc);
	~CDemoPlayer() override;

	void Construct(class CSnapshotDelta *pSnapshotDelta, bool UseVideo);

	void SetListener(IListener *pListener);

	int Load(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, int StorageType);
	unsigned char *GetMapData(class IStorage *pStorage);
	bool ExtractMap(class IStorage *pStorage);
	void Play();
	void Pause() override;
	void Unpause() override;
	void Stop(const char *pErrorMessage = "");
	void SetSpeed(float Speed) override;
	void SetSpeedIndex(int SpeedIndex) override;
	void AdjustSpeedIndex(int Offset) override;
	bool SeekPercent(float Percent) override;
	bool SeekTime(float Seconds) override;
	bool SeekTick(ETickOffset TickOffset) override;
	bool SetPos(int WantedTick) override;
	const CInfo *BaseInfo() const override { return &m_Info.m_Info; }
	void GetDemoName(char *pBuffer, size_t BufferSize) const override;
	bool GetDemoInfo(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, int StorageType, CDemoHeader *pDemoHeader, CTimelineMarkers *pTimelineMarkers, CMapInfo *pMapInfo, IOHANDLE *pFile = nullptr, char *pErrorMessage = nullptr, size_t ErrorMessageSize = 0) const override;
	const char *Filename() const { return m_aFilename; }
	const char *ErrorMessage() const override { return m_aErrorMessage; }

	void Update(bool RealTime = true);
	bool IsSixup() const { return m_Sixup; }

	const CPlaybackInfo *Info() const { return &m_Info; }
	bool IsPlaying() const override { return m_File != nullptr; }
	const CMapInfo *GetMapInfo() const { return &m_MapInfo; }
};

class CDemoEditor : public IDemoEditor
{
	IConsole *m_pConsole;
	IStorage *m_pStorage;
	class CSnapshotDelta *m_pSnapshotDelta;

public:
	virtual void Init(class CSnapshotDelta *pSnapshotDelta, class IConsole *pConsole, class IStorage *pStorage);
	bool Slice(const char *pDemo, const char *pDst, int StartTick, int EndTick, DEMOFUNC_FILTER pfnFilter, void *pUser) override;
};

#endif
