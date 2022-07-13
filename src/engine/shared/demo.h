/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_DEMO_H
#define ENGINE_SHARED_DEMO_H

#include <base/hash.h>

#include <engine/demo.h>
#include <engine/shared/protocol.h>
#include <functional>

#include "snapshot.h"

typedef std::function<void()> TUpdateIntraTimesFunc;

class CDemoRecorder : public IDemoRecorder
{
	class IConsole *m_pConsole = nullptr;
	IOHANDLE m_File;
	char m_aCurrentFilename[256] = {0};
	int m_LastTickMarker = 0;
	int m_LastKeyFrame = 0;
	int m_FirstTick = 0;
	unsigned char m_aLastSnapshotData[CSnapshot::MAX_SIZE] = {0};
	class CSnapshotDelta *m_pSnapshotDelta = nullptr;
	int m_NumTimelineMarkers = 0;
	int m_aTimelineMarkers[MAX_TIMELINE_MARKERS] = {0};
	bool m_NoMapData = false;
	unsigned char *m_pMapData = nullptr;

	DEMOFUNC_FILTER m_pfnFilter = nullptr;
	void *m_pUser = nullptr;

	void WriteTickMarker(int Tick, int Keyframe);
	void Write(int Type, const void *pData, int Size);

public:
	CDemoRecorder(class CSnapshotDelta *pSnapshotDelta, bool NoMapData = false);
	CDemoRecorder() {}

	int Start(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, const char *pNetversion, const char *pMap, SHA256_DIGEST *pSha256, unsigned MapCrc, const char *pType, unsigned MapSize, unsigned char *pMapData, IOHANDLE MapFile = nullptr, DEMOFUNC_FILTER pfnFilter = nullptr, void *pUser = nullptr);
	int Stop() override;
	void AddDemoMarker();

	void RecordSnapshot(int Tick, const void *pData, int Size);
	void RecordMessage(const void *pData, int Size);

	bool IsRecording() const override { return m_File != nullptr; }
	char *GetCurrentFilename() override { return m_aCurrentFilename; }

	int Length() const override { return (m_LastTickMarker - m_FirstTick) / SERVER_TICK_SPEED; }
};

class CDemoPlayer : public IDemoPlayer
{
public:
	class IListener
	{
	public:
		virtual ~IListener() {}
		virtual void OnDemoPlayerSnapshot(void *pData, int Size) = 0;
		virtual void OnDemoPlayerMessage(void *pData, int Size) = 0;
	};

	struct CPlaybackInfo
	{
		CDemoHeader m_Header;
		CTimelineMarkers m_TimelineMarkers;

		IDemoPlayer::CInfo m_Info;

		int64_t m_LastUpdate = 0;
		int64_t m_CurrentTime = 0;

		int m_SeekablePoints = 0;

		int m_NextTick = 0;
		int m_PreviousTick = 0;

		float m_IntraTick = 0;
		float m_IntraTickSincePrev = 0;
		float m_TickTime = 0;
	};

private:
	IListener *m_pListener = nullptr;

	TUpdateIntraTimesFunc m_UpdateIntraTimesFunc;

	// Playback
	struct CKeyFrame
	{
		long m_Filepos = 0;
		int m_Tick = 0;
	};

	struct CKeyFrameSearch
	{
		CKeyFrame m_Frame;
		CKeyFrameSearch *m_pNext = nullptr;
	};

	class IConsole *m_pConsole = nullptr;
	IOHANDLE m_File;
	long m_MapOffset = 0;
	char m_aFilename[IO_MAX_PATH_LENGTH] = {0};
	CKeyFrame *m_pKeyFrames = nullptr;
	CMapInfo m_MapInfo;
	int m_SpeedIndex = 0;

	CPlaybackInfo m_Info;
	int m_DemoType = 0;
	unsigned char m_aLastSnapshotData[CSnapshot::MAX_SIZE] = {0};
	int m_LastSnapshotDataSize = 0;
	class CSnapshotDelta *m_pSnapshotDelta = nullptr;

	int ReadChunkHeader(int *pType, int *pSize, int *pTick);
	void DoTick();
	void ScanFile();
	int NextFrame();

	int64_t time();

public:
	CDemoPlayer(class CSnapshotDelta *pSnapshotDelta);
	CDemoPlayer(class CSnapshotDelta *pSnapshotDelta, TUpdateIntraTimesFunc &&UpdateIntraTimesFunc);

	void Construct(class CSnapshotDelta *pSnapshotDelta);

	void SetListener(IListener *pListener);

	int Load(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, int StorageType);
	unsigned char *GetMapData(class IStorage *pStorage);
	bool ExtractMap(class IStorage *pStorage);
	int Play();
	void Pause() override;
	void Unpause() override;
	int Stop();
	void SetSpeed(float Speed) override;
	void SetSpeedIndex(int Offset) override;
	int SeekPercent(float Percent) override;
	int SeekTime(float Seconds) override;
	int SetPos(int WantedTick) override;
	const CInfo *BaseInfo() const override { return &m_Info.m_Info; }
	void GetDemoName(char *pBuffer, int BufferSize) const override;
	bool GetDemoInfo(class IStorage *pStorage, const char *pFilename, int StorageType, CDemoHeader *pDemoHeader, CTimelineMarkers *pTimelineMarkers, CMapInfo *pMapInfo) const override;
	const char *GetDemoFileName() { return m_aFilename; }
	int GetDemoType() const override;

	int Update(bool RealTime = true);

	const CPlaybackInfo *Info() const { return &m_Info; }
	bool IsPlaying() const override { return m_File != nullptr; }
	const CMapInfo *GetMapInfo() const { return &m_MapInfo; }
};

class CDemoEditor : public IDemoEditor, public CDemoPlayer::IListener
{
	CDemoPlayer *m_pDemoPlayer = nullptr;
	CDemoRecorder *m_pDemoRecorder = nullptr;
	IConsole *m_pConsole = nullptr;
	IStorage *m_pStorage = nullptr;
	class CSnapshotDelta *m_pSnapshotDelta = nullptr;
	const char *m_pNetVersion = nullptr;

	bool m_Stop = false;
	int m_SliceFrom = 0;
	int m_SliceTo = 0;

public:
	virtual void Init(const char *pNetVersion, class CSnapshotDelta *pSnapshotDelta, class IConsole *pConsole, class IStorage *pStorage);
	void Slice(const char *pDemo, const char *pDst, int StartTick, int EndTick, DEMOFUNC_FILTER pfnFilter, void *pUser) override;

	void OnDemoPlayerSnapshot(void *pData, int Size) override;
	void OnDemoPlayerMessage(void *pData, int Size) override;
};

#endif
