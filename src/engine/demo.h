/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_DEMO_H
#define ENGINE_DEMO_H

#include "kernel.h"
#include <base/hash.h>
#include <engine/map.h>
#include <engine/shared/uuid_manager.h>

enum
{
	MAX_TIMELINE_MARKERS = 64
};

constexpr int g_DemoSpeeds = 22;
extern const double g_aSpeeds[g_DemoSpeeds];

typedef bool (*DEMOFUNC_FILTER)(const void *pData, int DataSize, void *pUser);

// TODO: Properly extend demo format using uuids
// "6be6da4a-cebd-380c-9b5b-1289c842d780"
// "demoitem-sha256@ddnet.tw"
extern const CUuid SHA256_EXTENSION;

struct CDemoHeader
{
	unsigned char m_aMarker[7] = {0};
	unsigned char m_Version = 0;
	char m_aNetversion[64] = {0};
	char m_aMapName[64] = {0};
	unsigned char m_aMapSize[4] = {0};
	unsigned char m_aMapCrc[4] = {0};
	char m_aType[8] = {0};
	unsigned char m_aLength[4] = {0};
	char m_aTimestamp[20] = {0};
};

struct CTimelineMarkers
{
	unsigned char m_aNumTimelineMarkers[4] = {0};
	unsigned char m_aaTimelineMarkers[MAX_TIMELINE_MARKERS][4] = {{0}};
};

struct CMapInfo
{
	char m_aName[MAX_MAP_LENGTH] = {0};
	SHA256_DIGEST m_Sha256;
	int m_Crc = 0;
	int m_Size = 0;
};

class IDemoPlayer : public IInterface
{
	MACRO_INTERFACE("demoplayer", 0)
public:
	class CInfo
	{
	public:
		bool m_Paused = false;
		float m_Speed = 0;

		int m_FirstTick = 0;
		int m_CurrentTick = 0;
		int m_LastTick = 0;

		int m_NumTimelineMarkers = 0;
		int m_aTimelineMarkers[MAX_TIMELINE_MARKERS] = {0};
	};

	enum
	{
		DEMOTYPE_INVALID = 0,
		DEMOTYPE_CLIENT,
		DEMOTYPE_SERVER,
	};

	~IDemoPlayer() {}
	virtual void SetSpeed(float Speed) = 0;
	virtual void SetSpeedIndex(int Offset) = 0;
	virtual int SeekPercent(float Percent) = 0;
	virtual int SeekTime(float Seconds) = 0;
	virtual int SetPos(int WantedTick) = 0;
	virtual void Pause() = 0;
	virtual void Unpause() = 0;
	virtual bool IsPlaying() const = 0;
	virtual const CInfo *BaseInfo() const = 0;
	virtual void GetDemoName(char *pBuffer, int BufferSize) const = 0;
	virtual bool GetDemoInfo(class IStorage *pStorage, const char *pFilename, int StorageType, CDemoHeader *pDemoHeader, CTimelineMarkers *pTimelineMarkers, CMapInfo *pMapInfo) const = 0;
	virtual int GetDemoType() const = 0;
};

class IDemoRecorder : public IInterface
{
	MACRO_INTERFACE("demorecorder", 0)
public:
	~IDemoRecorder() {}
	virtual bool IsRecording() const = 0;
	virtual int Stop() = 0;
	virtual int Length() const = 0;
	virtual char *GetCurrentFilename() = 0;
};

class IDemoEditor : public IInterface
{
	MACRO_INTERFACE("demoeditor", 0)
public:
	virtual void Slice(const char *pDemo, const char *pDst, int StartTick, int EndTick, DEMOFUNC_FILTER pfnFilter, void *pUser) = 0;
};

#endif
