#ifndef ENGINE_CLIENT_GHOST_H
#define ENGINE_CLIENT_GHOST_H

#include <engine/ghost.h>

#include <cstdint>

enum
{
	MAX_ITEM_SIZE = 128,
	NUM_ITEMS_PER_CHUNK = 50,
	MAX_CHUNK_SIZE = MAX_ITEM_SIZE * NUM_ITEMS_PER_CHUNK,
};
static_assert(MAX_CHUNK_SIZE % sizeof(uint32_t) == 0, "Chunk size must be aligned with uint32_t");

// version 4-6
struct CGhostHeader
{
	unsigned char m_aMarker[8];
	unsigned char m_Version;
	char m_aOwner[MAX_NAME_LENGTH];
	char m_aMap[64];
	unsigned char m_aZeroes[sizeof(int32_t)]; // Crc before version 6
	unsigned char m_aNumTicks[sizeof(int32_t)];
	unsigned char m_aTime[sizeof(int32_t)];
	SHA256_DIGEST m_MapSha256;

	int GetTicks() const;
	int GetTime() const;
	CGhostInfo ToGhostInfo() const;
};

class CGhostItem
{
public:
	alignas(uint32_t) unsigned char m_aData[MAX_ITEM_SIZE];
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
	char m_aFilename[IO_MAX_PATH_LENGTH];
	class IStorage *m_pStorage;

	alignas(uint32_t) char m_aBuffer[MAX_CHUNK_SIZE];
	alignas(uint32_t) char m_aBufferTemp[MAX_CHUNK_SIZE];
	char *m_pBufferPos;
	const char *m_pBufferEnd;
	int m_BufferNumItems;
	CGhostItem m_LastItem;

	void ResetBuffer();
	void FlushChunk();

public:
	CGhostRecorder();

	void Init();

	int Start(const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, const char *pName) override;
	void Stop(int Ticks, int Time) override;

	void WriteData(int Type, const void *pData, size_t Size) override;
	bool IsRecording() const override { return m_File != nullptr; }
};

class CGhostLoader : public IGhostLoader
{
	IOHANDLE m_File;
	char m_aFilename[IO_MAX_PATH_LENGTH];
	class IStorage *m_pStorage;

	CGhostHeader m_Header;
	CGhostInfo m_Info;

	alignas(uint32_t) char m_aBuffer[MAX_CHUNK_SIZE];
	alignas(uint32_t) char m_aBufferTemp[MAX_CHUNK_SIZE];
	char *m_pBufferPos;
	const char *m_pBufferEnd;
	int m_BufferNumItems;
	int m_BufferCurItem;
	int m_BufferPrevItem;
	CGhostItem m_LastItem;

	void ResetBuffer();
	IOHANDLE ReadHeader(CGhostHeader &Header, const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc, bool LogMapMismatch) const;
	bool ValidateHeader(const CGhostHeader &Header, const char *pFilename) const;
	bool CheckHeaderMap(const CGhostHeader &Header, const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc, bool LogMapMismatch) const;
	bool ReadChunk(int *pType);

public:
	CGhostLoader();

	void Init();

	bool Load(const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc) override;
	void Close() override;
	const CGhostInfo *GetInfo() const override { return &m_Info; }

	bool ReadNextType(int *pType) override;
	bool ReadData(int Type, void *pData, size_t Size) override;

	bool GetGhostInfo(const char *pFilename, CGhostInfo *pGhostInfo, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc) override;
};
#endif
