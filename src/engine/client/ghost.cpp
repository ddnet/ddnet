#include "ghost.h"

#include <base/log.h>
#include <base/system.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/storage.h>

static const unsigned char gs_aHeaderMarker[8] = {'T', 'W', 'G', 'H', 'O', 'S', 'T', 0};
static const unsigned char gs_CurVersion = 6;

static const LOG_COLOR LOG_COLOR_GHOST{165, 153, 153};

int CGhostHeader::GetTicks() const
{
	return bytes_be_to_uint(m_aNumTicks);
}

int CGhostHeader::GetTime() const
{
	return bytes_be_to_uint(m_aTime);
}

CGhostInfo CGhostHeader::ToGhostInfo() const
{
	CGhostInfo Result;
	str_copy(Result.m_aOwner, m_aOwner);
	str_copy(Result.m_aMap, m_aMap);
	Result.m_NumTicks = GetTicks();
	Result.m_Time = GetTime();
	return Result;
}

CGhostRecorder::CGhostRecorder()
{
	m_File = nullptr;
	m_aFilename[0] = '\0';
	ResetBuffer();
}

void CGhostRecorder::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
}

int CGhostRecorder::Start(const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, const char *pName)
{
	dbg_assert(!m_File, "File already open");

	m_File = m_pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!m_File)
	{
		log_info_color(LOG_COLOR_GHOST, "ghost_recorder", "Unable to open '%s' for recording", pFilename);
		return -1;
	}
	str_copy(m_aFilename, pFilename);

	// write header
	CGhostHeader Header;
	mem_zero(&Header, sizeof(Header));
	mem_copy(Header.m_aMarker, gs_aHeaderMarker, sizeof(Header.m_aMarker));
	Header.m_Version = gs_CurVersion;
	str_copy(Header.m_aOwner, pName);
	str_copy(Header.m_aMap, pMap);
	Header.m_MapSha256 = MapSha256;
	io_write(m_File, &Header, sizeof(Header));

	m_LastItem.Reset();
	ResetBuffer();

	log_info_color(LOG_COLOR_GHOST, "ghost_recorder", "Recording to '%s'", pFilename);
	return 0;
}

void CGhostRecorder::ResetBuffer()
{
	m_pBufferPos = m_aBuffer;
	m_pBufferEnd = m_aBuffer;
	m_BufferNumItems = 0;
}

static void DiffItem(const uint32_t *pPast, const uint32_t *pCurrent, uint32_t *pOut, size_t Size)
{
	while(Size)
	{
		*pOut = *pCurrent - *pPast;
		pOut++;
		pPast++;
		pCurrent++;
		Size--;
	}
}

void CGhostRecorder::WriteData(int Type, const void *pData, size_t Size)
{
	dbg_assert((bool)m_File, "File not open");
	dbg_assert(Type >= 0 && Type <= (int)std::numeric_limits<unsigned char>::max(), "Type invalid");
	dbg_assert(Size > 0 && Size <= MAX_ITEM_SIZE && Size % sizeof(uint32_t) == 0, "Size invalid");

	if((size_t)(m_pBufferEnd - m_pBufferPos) < Size)
	{
		FlushChunk();
	}

	CGhostItem Data(Type);
	mem_copy(Data.m_aData, pData, Size);
	if(m_LastItem.m_Type == Data.m_Type)
	{
		DiffItem((const uint32_t *)m_LastItem.m_aData, (const uint32_t *)Data.m_aData, (uint32_t *)m_pBufferPos, Size / sizeof(uint32_t));
	}
	else
	{
		FlushChunk();
		mem_copy(m_pBufferPos, Data.m_aData, Size);
	}

	m_LastItem = Data;
	m_pBufferPos += Size;
	m_BufferNumItems++;
	if(m_BufferNumItems >= NUM_ITEMS_PER_CHUNK)
	{
		FlushChunk();
	}
}

void CGhostRecorder::FlushChunk()
{
	dbg_assert((bool)m_File, "File not open");

	int Size = m_pBufferPos - m_aBuffer;
	if(Size == 0 || m_BufferNumItems == 0)
	{
		return;
	}
	dbg_assert(Size % sizeof(uint32_t) == 0, "Chunk size invalid");

	Size = CVariableInt::Compress(m_aBuffer, Size, m_aBufferTemp, sizeof(m_aBufferTemp));
	if(Size < 0)
	{
		log_info_color(LOG_COLOR_GHOST, "ghost_recorder", "Failed to write chunk to '%s': error during intpack compression", m_aFilename);
		m_LastItem.Reset();
		ResetBuffer();
		return;
	}

	Size = CNetBase::Compress(m_aBufferTemp, Size, m_aBuffer, sizeof(m_aBuffer));
	if(Size < 0)
	{
		log_info_color(LOG_COLOR_GHOST, "ghost_recorder", "Failed to write chunk to '%s': error during network compression", m_aFilename);
		m_LastItem.Reset();
		ResetBuffer();
		return;
	}

	unsigned char aChunkHeader[4];
	aChunkHeader[0] = m_LastItem.m_Type & 0xff;
	aChunkHeader[1] = m_BufferNumItems & 0xff;
	aChunkHeader[2] = (Size >> 8) & 0xff;
	aChunkHeader[3] = Size & 0xff;

	io_write(m_File, aChunkHeader, sizeof(aChunkHeader));
	io_write(m_File, m_aBuffer, Size);

	m_LastItem.Reset();
	ResetBuffer();
}

void CGhostRecorder::Stop(int Ticks, int Time)
{
	if(!m_File)
	{
		return;
	}

	const bool DiscardFile = Ticks <= 0 || Time <= 0;

	if(!DiscardFile)
	{
		FlushChunk();

		// write number of ticks and time
		io_seek(m_File, offsetof(CGhostHeader, m_aNumTicks), IOSEEK_START);

		unsigned char aNumTicks[sizeof(int32_t)];
		uint_to_bytes_be(aNumTicks, Ticks);
		io_write(m_File, aNumTicks, sizeof(aNumTicks));

		unsigned char aTime[sizeof(int32_t)];
		uint_to_bytes_be(aTime, Time);
		io_write(m_File, aTime, sizeof(aTime));
	}

	io_close(m_File);
	m_File = nullptr;

	if(DiscardFile)
	{
		m_pStorage->RemoveFile(m_aFilename, IStorage::TYPE_SAVE);
	}

	log_info_color(LOG_COLOR_GHOST, "ghost_recorder", "Stopped recording to '%s'", m_aFilename);
	m_aFilename[0] = '\0';
}

CGhostLoader::CGhostLoader()
{
	m_File = nullptr;
	m_aFilename[0] = '\0';
	ResetBuffer();
}

void CGhostLoader::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
}

void CGhostLoader::ResetBuffer()
{
	m_pBufferPos = m_aBuffer;
	m_pBufferEnd = m_aBuffer;
	m_BufferNumItems = 0;
	m_BufferCurItem = 0;
	m_BufferPrevItem = -1;
}

IOHANDLE CGhostLoader::ReadHeader(CGhostHeader &Header, const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc, bool LogMapMismatch) const
{
	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to open ghost file '%s' for reading", pFilename);
		return nullptr;
	}

	if(io_read(File, &Header, sizeof(Header)) != sizeof(Header))
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': failed to read header", pFilename);
		io_close(File);
		return nullptr;
	}

	if(!ValidateHeader(Header, pFilename) ||
		!CheckHeaderMap(Header, pFilename, pMap, MapSha256, MapCrc, LogMapMismatch))
	{
		io_close(File);
		return nullptr;
	}

	return File;
}

bool CGhostLoader::ValidateHeader(const CGhostHeader &Header, const char *pFilename) const
{
	if(mem_comp(Header.m_aMarker, gs_aHeaderMarker, sizeof(gs_aHeaderMarker)) != 0)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': invalid header marker", pFilename);
		return false;
	}

	if(Header.m_Version < 4 || Header.m_Version > gs_CurVersion)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': ghost version '%d' is not supported", pFilename, Header.m_Version);
		return false;
	}

	if(!mem_has_null(Header.m_aOwner, sizeof(Header.m_aOwner)) || !str_utf8_check(Header.m_aOwner))
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': owner name is invalid", pFilename);
		return false;
	}

	if(!mem_has_null(Header.m_aMap, sizeof(Header.m_aMap)) || !str_utf8_check(Header.m_aMap))
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': map name is invalid", pFilename);
		return false;
	}

	const int NumTicks = Header.GetTicks();
	if(NumTicks <= 0)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': number of ticks '%d' is invalid", pFilename, NumTicks);
		return false;
	}

	const int Time = Header.GetTime();
	if(Time <= 0)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': time '%d' is invalid", pFilename, Time);
		return false;
	}

	return true;
}

bool CGhostLoader::CheckHeaderMap(const CGhostHeader &Header, const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc, bool LogMapMismatch) const
{
	if(str_comp(Header.m_aMap, pMap) != 0)
	{
		if(LogMapMismatch)
		{
			log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': ghost map name '%s' does not match current map '%s'", pFilename, Header.m_aMap, pMap);
		}
		return false;
	}

	if(Header.m_Version >= 6)
	{
		if(Header.m_MapSha256 != MapSha256 && g_Config.m_ClRaceGhostStrictMap)
		{
			if(LogMapMismatch)
			{
				char aGhostSha256[SHA256_MAXSTRSIZE];
				sha256_str(Header.m_MapSha256, aGhostSha256, sizeof(aGhostSha256));
				char aMapSha256[SHA256_MAXSTRSIZE];
				sha256_str(MapSha256, aMapSha256, sizeof(aMapSha256));
				log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': ghost map SHA256 mismatch (wanted='%s', ghost='%s')", pFilename, aMapSha256, aGhostSha256);
			}
			return false;
		}
	}
	else
	{
		const unsigned GhostMapCrc = bytes_be_to_uint(Header.m_aZeroes);
		if(GhostMapCrc != MapCrc && g_Config.m_ClRaceGhostStrictMap)
		{
			if(LogMapMismatch)
			{
				log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': ghost map CRC mismatch (wanted='%08x', ghost='%08x')", pFilename, MapCrc, GhostMapCrc);
			}
			return false;
		}
	}

	return true;
}

bool CGhostLoader::Load(const char *pFilename, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc)
{
	dbg_assert(!m_File, "File already open");

	CGhostHeader Header;
	IOHANDLE File = ReadHeader(Header, pFilename, pMap, MapSha256, MapCrc, true);
	if(!File)
	{
		return false;
	}

	if(Header.m_Version < 6)
	{
		io_skip(File, -(int)sizeof(SHA256_DIGEST));
	}

	m_File = File;
	str_copy(m_aFilename, pFilename);
	m_Header = Header;
	m_Info = m_Header.ToGhostInfo();
	m_LastItem.Reset();
	ResetBuffer();
	return true;
}

bool CGhostLoader::ReadChunk(int *pType)
{
	if(m_Header.m_Version != 4)
	{
		m_LastItem.Reset();
	}
	ResetBuffer();

	unsigned char aChunkHeader[4];
	if(io_read(m_File, aChunkHeader, sizeof(aChunkHeader)) != sizeof(aChunkHeader))
	{
		return false; // EOF
	}

	*pType = aChunkHeader[0];
	int Size = (aChunkHeader[2] << 8) | aChunkHeader[3];
	m_BufferNumItems = aChunkHeader[1];

	if(Size <= 0 || Size > MAX_CHUNK_SIZE)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': invalid chunk header size", m_aFilename);
		return false;
	}

	if(io_read(m_File, m_aBuffer, Size) != (unsigned)Size)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': error reading chunk data", m_aFilename);
		return false;
	}

	Size = CNetBase::Decompress(m_aBuffer, Size, m_aBufferTemp, sizeof(m_aBufferTemp));
	if(Size < 0)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': error during network decompression", m_aFilename);
		return false;
	}

	Size = CVariableInt::Decompress(m_aBufferTemp, Size, m_aBuffer, sizeof(m_aBuffer));
	if(Size < 0)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': error during intpack decompression", m_aFilename);
		return false;
	}

	m_pBufferEnd = m_aBuffer + Size;
	return true;
}

bool CGhostLoader::ReadNextType(int *pType)
{
	dbg_assert((bool)m_File, "File not open");

	if(m_BufferCurItem != m_BufferPrevItem && m_BufferCurItem < m_BufferNumItems)
	{
		*pType = m_LastItem.m_Type;
	}
	else if(!ReadChunk(pType))
	{
		return false; // error or EOF
	}

	m_BufferPrevItem = m_BufferCurItem;
	return true;
}

static void UndiffItem(const uint32_t *pPast, const uint32_t *pDiff, uint32_t *pOut, size_t Size)
{
	while(Size)
	{
		*pOut = *pPast + *pDiff;
		pOut++;
		pPast++;
		pDiff++;
		Size--;
	}
}

bool CGhostLoader::ReadData(int Type, void *pData, size_t Size)
{
	dbg_assert((bool)m_File, "File not open");
	dbg_assert(Type >= 0 && Type <= (int)std::numeric_limits<unsigned char>::max(), "Type invalid");
	dbg_assert(Size > 0 && Size <= MAX_ITEM_SIZE && Size % sizeof(uint32_t) == 0, "Size invalid");

	if((size_t)(m_pBufferEnd - m_pBufferPos) < Size)
	{
		log_error_color(LOG_COLOR_GHOST, "ghost_loader", "Failed to read ghost file '%s': not enough data (type='%d', got='%" PRIzu "', wanted='%" PRIzu "')", m_aFilename, Type, (size_t)(m_pBufferEnd - m_pBufferPos), Size);
		return false;
	}

	CGhostItem Data(Type);
	if(m_LastItem.m_Type == Data.m_Type)
	{
		UndiffItem((const uint32_t *)m_LastItem.m_aData, (const uint32_t *)m_pBufferPos, (uint32_t *)Data.m_aData, Size / sizeof(uint32_t));
	}
	else
	{
		mem_copy(Data.m_aData, m_pBufferPos, Size);
	}

	mem_copy(pData, Data.m_aData, Size);

	m_LastItem = Data;
	m_pBufferPos += Size;
	m_BufferCurItem++;
	return true;
}

void CGhostLoader::Close()
{
	if(!m_File)
	{
		return;
	}

	io_close(m_File);
	m_File = nullptr;
	m_aFilename[0] = '\0';
}

bool CGhostLoader::GetGhostInfo(const char *pFilename, CGhostInfo *pGhostInfo, const char *pMap, const SHA256_DIGEST &MapSha256, unsigned MapCrc)
{
	CGhostHeader Header;
	IOHANDLE File = ReadHeader(Header, pFilename, pMap, MapSha256, MapCrc, false);
	if(!File)
	{
		return false;
	}
	io_close(File);
	*pGhostInfo = Header.ToGhostInfo();
	return true;
}
