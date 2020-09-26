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

const double g_aSpeeds[] = {0.1, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 20.0, 24.0, 28.0, 32.0, 40.0, 48.0, 56.0, 64.0};

typedef bool (*DEMOFUNC_FILTER)(const void *pData, int DataSize, void *pUser);

// TODO: Properly extend demo format using uuids
// "6be6da4a-cebd-380c-9b5b-1289c842d780"
// "demoitem-sha256@ddnet.tw"
static const CUuid SHA256_EXTENSION =
	{{0x6b, 0xe6, 0xda, 0x4a, 0xce, 0xbd, 0x38, 0x0c,
		0x9b, 0x5b, 0x12, 0x89, 0xc8, 0x42, 0xd7, 0x80}};

struct CDemoHeader
{
	unsigned char m_aMarker[7];
	unsigned char m_Version;
	char m_aNetversion[64];
	char m_aMapName[64];
	unsigned char m_aMapSize[4];
	unsigned char m_aMapCrc[4];
	char m_aType[8];
	char m_aLength[4];
	char m_aTimestamp[20];
};

struct CTimelineMarkers
{
	char m_aNumTimelineMarkers[4];
	char m_aTimelineMarkers[MAX_TIMELINE_MARKERS][4];
};

struct CMapInfo
{
	char m_aName[MAX_MAP_LENGTH];
	SHA256_DIGEST m_Sha256;
	int m_Crc;
	int m_Size;
};

class IDemoPlayer : public IInterface
{
	MACRO_INTERFACE("demoplayer", 0)
public:
	class CInfo
	{
	public:
		bool m_Paused;
		float m_Speed;

		int m_FirstTick;
		int m_CurrentTick;
		int m_LastTick;

		int m_NumTimelineMarkers;
		int m_aTimelineMarkers[MAX_TIMELINE_MARKERS];
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
