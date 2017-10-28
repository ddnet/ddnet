#ifndef ENGINE_GHOST_H
#define ENGINE_GHOST_H

#include <engine/shared/protocol.h>

#include "kernel.h"

class CGhostHeader
{
public:
	unsigned char m_aMarker[8];
	unsigned char m_Version;
	char m_aOwner[MAX_NAME_LENGTH];
	char m_aMap[64];
	unsigned char m_aCrc[4];
	unsigned char m_aNumTicks[4];
	unsigned char m_aTime[4];

	int GetTime() const
	{
		return (m_aTime[0] << 24) | (m_aTime[1] << 16) | (m_aTime[2] << 8) | (m_aTime[3]);
	}

	int GetTicks() const
	{
		return (m_aNumTicks[0] << 24) | (m_aNumTicks[1] << 16) | (m_aNumTicks[2] << 8) | (m_aNumTicks[3]);
	}
};

class IGhostRecorder : public IInterface
{
	MACRO_INTERFACE("ghostrecorder", 0)
public:
	virtual ~IGhostRecorder() {}

	virtual int Start(const char *pFilename, const char *pMap, unsigned MapCrc, const char *pName) = 0;
	virtual int Stop(int Ticks, int Time) = 0;

	virtual void WriteData(int Type, const void *pData, int Size) = 0;
	virtual bool IsRecording() const = 0;
};

class IGhostLoader : public IInterface
{
	MACRO_INTERFACE("ghostloader", 0)
public:
	virtual ~IGhostLoader() {}

	virtual int Load(const char *pFilename, const char *pMap, unsigned Crc) = 0;
	virtual void Close() = 0;

	virtual const CGhostHeader *GetHeader() const = 0;

	virtual bool ReadNextType(int *pType) = 0;
	virtual bool ReadData(int Type, void *pData, int Size) = 0;

	virtual bool GetGhostInfo(const char *pFilename, CGhostHeader *pGhostHeader, const char *pMap, unsigned Crc) = 0;
};

#endif
