#ifndef ENGINE_CLIENT_GHOST_H
#define ENGINE_CLIENT_GHOST_H

#include <engine/ghost.h>

enum
{
	MAX_ITEM_SIZE = 128,
	NUM_ITEMS_PER_CHUNK = 50,
};

// version 4-6
struct CGhostHeader
{
	unsigned char m_aMarker[8];
	unsigned char m_Version;
	char m_aOwner[MAX_NAME_LENGTH];
	char m_aMap[64];
	unsigned char m_aZeroes[4]; // Crc before version 6
	unsigned char m_aNumTicks[4];
	unsigned char m_aTime[4];
	SHA256_DIGEST m_MapSha256;

	int GetTicks() const
	{
		return bytes_be_to_int(m_aNumTicks);
	}

	int GetTime() const
	{
		return bytes_be_to_int(m_aTime);
	}

	CGhostInfo ToGhostInfo() const
	{
		CGhostInfo Result;
		mem_zero(&Result, sizeof(Result));
		str_copy(Result.m_aOwner, m_aOwner);
		str_copy(Result.m_aMap, m_aMap);
		Result.m_NumTicks = GetTicks();
		Result.m_Time = GetTime();
		return Result;
	}
};

class CGhostItem
{
public:
	unsigned char m_aData[MAX_ITEM_SIZE];
	int m_Type;

	CGhostItem() :
		m_Type(-1) {}
	CGhostItem(int Type) :
		m_Type(Type) {}
	void Reset() { m_Type = -1; }
};

class CGhostRecorder : public IGhostRecorder
{
	IOHANDLE m_File;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	CGhostItem m_LastItem;

	char m_aBuffer[MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK];
	char *m_pBufferPos;
	int m_BufferNumItems;

	void ResetBuffer();
	void FlushChunk();

public:
	CGhostRecorder();

	void Init();

	int Start(const char *pFilename, const char *pMap, SHA256_DIGEST MapSha256, const char *pName) override;
	int Stop(int Ticks, int Time) override;

	void WriteData(int Type, const void *pData, int Size) override;
	bool IsRecording() const override { return m_File != nullptr; }
};

class CGhostLoader : public IGhostLoader
{
	IOHANDLE m_File;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;

	CGhostHeader m_Header;
	CGhostInfo m_Info;

	CGhostItem m_LastItem;

	char m_aBuffer[MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK];
	char *m_pBufferPos;
	int m_BufferNumItems;
	int m_BufferCurItem;
	int m_BufferPrevItem;

	void ResetBuffer();
	int ReadChunk(int *pType);

public:
	CGhostLoader();

	void Init();

	int Load(const char *pFilename, const char *pMap, SHA256_DIGEST MapSha256, unsigned MapCrc) override;
	void Close() override;
	const CGhostInfo *GetInfo() const override { return &m_Info; }

	bool ReadNextType(int *pType) override;
	bool ReadData(int Type, void *pData, int Size) override;

	bool GetGhostInfo(const char *pFilename, CGhostInfo *pGhostInfo, const char *pMap, SHA256_DIGEST MapSha256, unsigned MapCrc) override;
};
#endif
