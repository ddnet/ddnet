/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/storage.h>

#include <engine/shared/config.h>

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif

#include "compression.h"
#include "demo.h"
#include "network.h"
#include "snapshot.h"

const double g_aSpeeds[g_DemoSpeeds] = {0.1, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 20.0, 24.0, 28.0, 32.0, 40.0, 48.0, 56.0, 64.0};
const CUuid SHA256_EXTENSION =
	{{0x6b, 0xe6, 0xda, 0x4a, 0xce, 0xbd, 0x38, 0x0c,
		0x9b, 0x5b, 0x12, 0x89, 0xc8, 0x42, 0xd7, 0x80}};

static const unsigned char gs_CurVersion = 6;
static const unsigned char gs_OldVersion = 3;
static const unsigned char gs_Sha256Version = 6;
static const unsigned char gs_VersionTickCompression = 5; // demo files with this version or higher will use `CHUNKTICKFLAG_TICK_COMPRESSED`

static const ColorRGBA gs_DemoPrintColor{0.75f, 0.7f, 0.7f, 1.0f};

bool CDemoHeader::Valid() const
{
	// Check marker and ensure that strings are zero-terminated and valid UTF-8.
	return mem_comp(m_aMarker, gs_aHeaderMarker, sizeof(gs_aHeaderMarker)) == 0 &&
	       mem_has_null(m_aNetversion, sizeof(m_aNetversion)) && str_utf8_check(m_aNetversion) &&
	       mem_has_null(m_aMapName, sizeof(m_aMapName)) && str_utf8_check(m_aMapName) &&
	       mem_has_null(m_aType, sizeof(m_aType)) && str_utf8_check(m_aType) &&
	       mem_has_null(m_aTimestamp, sizeof(m_aTimestamp)) && str_utf8_check(m_aTimestamp);
}

CDemoRecorder::CDemoRecorder(class CSnapshotDelta *pSnapshotDelta, bool NoMapData)
{
	m_File = 0;
	m_aCurrentFilename[0] = '\0';
	m_pfnFilter = nullptr;
	m_pUser = nullptr;
	m_LastTickMarker = -1;
	m_pSnapshotDelta = pSnapshotDelta;
	m_NoMapData = NoMapData;
}

CDemoRecorder::~CDemoRecorder()
{
	dbg_assert(m_File == 0, "Demo recorder was not stopped");
}

// Record
int CDemoRecorder::Start(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, const char *pNetVersion, const char *pMap, const SHA256_DIGEST &Sha256, unsigned Crc, const char *pType, unsigned MapSize, unsigned char *pMapData, IOHANDLE MapFile, DEMOFUNC_FILTER pfnFilter, void *pUser)
{
	dbg_assert(m_File == 0, "Demo recorder already recording");

	m_pConsole = pConsole;
	m_pStorage = pStorage;

	IOHANDLE DemoFile = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!DemoFile)
	{
		if(m_pConsole)
		{
			char aBuf[64 + IO_MAX_PATH_LENGTH];
			str_format(aBuf, sizeof(aBuf), "Unable to open '%s' for recording", pFilename);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
		}
		return -1;
	}

	bool CloseMapFile = false;

	if(MapFile)
		io_seek(MapFile, 0, IOSEEK_START);

	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(Sha256, aSha256, sizeof(aSha256));

	if(!pMapData && !MapFile)
	{
		// open mapfile
		char aMapFilename[IO_MAX_PATH_LENGTH];
		// try the downloaded maps
		str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s_%s.map", pMap, aSha256);
		MapFile = pStorage->OpenFile(aMapFilename, IOFLAG_READ, IStorage::TYPE_ALL);
		if(!MapFile)
		{
			// try the normal maps folder
			str_format(aMapFilename, sizeof(aMapFilename), "maps/%s.map", pMap);
			MapFile = pStorage->OpenFile(aMapFilename, IOFLAG_READ, IStorage::TYPE_ALL);
		}
		if(!MapFile)
		{
			// search for the map within subfolders
			char aBuf[IO_MAX_PATH_LENGTH];
			str_format(aMapFilename, sizeof(aMapFilename), "%s.map", pMap);
			if(pStorage->FindFile(aMapFilename, "maps", IStorage::TYPE_ALL, aBuf, sizeof(aBuf)))
				MapFile = pStorage->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		}
		if(!MapFile)
		{
			if(m_pConsole)
			{
				char aBuf[32 + IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "Unable to open mapfile '%s'", pMap);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
			}
			return -1;
		}

		CloseMapFile = true;
	}

	if(m_NoMapData)
	{
		MapSize = 0;
	}
	else if(MapFile)
	{
		const int64_t MapFileSize = io_length(MapFile);
		if(MapFileSize > (int64_t)std::numeric_limits<unsigned>::max())
		{
			if(CloseMapFile)
			{
				io_close(MapFile);
			}
			MapSize = 0;
			if(m_pConsole)
			{
				char aBuf[32 + IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "Mapfile '%s' too large for demo, recording without it", pMap);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
			}
		}
		else
		{
			MapSize = MapFileSize;
		}
	}

	// write header
	CDemoHeader Header;
	mem_zero(&Header, sizeof(Header));
	mem_copy(Header.m_aMarker, gs_aHeaderMarker, sizeof(Header.m_aMarker));
	Header.m_Version = gs_CurVersion;
	str_copy(Header.m_aNetversion, pNetVersion);
	str_copy(Header.m_aMapName, pMap);
	uint_to_bytes_be(Header.m_aMapSize, MapSize);
	uint_to_bytes_be(Header.m_aMapCrc, Crc);
	str_copy(Header.m_aType, pType);
	// Header.m_Length - add this on stop
	str_timestamp(Header.m_aTimestamp, sizeof(Header.m_aTimestamp));
	io_write(DemoFile, &Header, sizeof(Header));

	CTimelineMarkers TimelineMarkers;
	mem_zero(&TimelineMarkers, sizeof(TimelineMarkers));
	io_write(DemoFile, &TimelineMarkers, sizeof(TimelineMarkers)); // fill this on stop

	// Write Sha256
	io_write(DemoFile, SHA256_EXTENSION.m_aData, sizeof(SHA256_EXTENSION.m_aData));
	io_write(DemoFile, &Sha256, sizeof(SHA256_DIGEST));

	if(MapSize == 0)
	{
	}
	else if(pMapData)
	{
		io_write(DemoFile, pMapData, MapSize);
	}
	else
	{
		// write map data
		while(true)
		{
			unsigned char aChunk[1024 * 64];
			mem_zero(aChunk, sizeof(aChunk));
			int Bytes = io_read(MapFile, &aChunk, sizeof(aChunk));
			if(Bytes <= 0)
				break;
			io_write(DemoFile, &aChunk, Bytes);
		}
		if(CloseMapFile)
			io_close(MapFile);
		else
			io_seek(MapFile, 0, IOSEEK_START);
	}

	m_LastKeyFrame = -1;
	m_LastTickMarker = -1;
	m_FirstTick = -1;
	m_NumTimelineMarkers = 0;

	if(m_pConsole)
	{
		char aBuf[32 + IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "Recording to '%s'", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
	}

	m_pfnFilter = pfnFilter;
	m_pUser = pUser;

	m_File = DemoFile;
	str_copy(m_aCurrentFilename, pFilename);

	return 0;
}

/*
	Tickmarker
		7	= Always set
		6	= Keyframe flag
		0-5	= Delta tick

	Normal
		7 = Not set
		5-6	= Type
		0-4	= Size
*/

enum
{
	CHUNKTYPEFLAG_TICKMARKER = 0x80,
	CHUNKTICKFLAG_KEYFRAME = 0x40, // only when tickmarker is set
	CHUNKTICKFLAG_TICK_COMPRESSED = 0x20, // when we store the tick value in the first chunk

	CHUNKMASK_TICK = 0x1f,
	CHUNKMASK_TICK_LEGACY = 0x3f,
	CHUNKMASK_TYPE = 0x60,
	CHUNKMASK_SIZE = 0x1f,

	CHUNKTYPE_SNAPSHOT = 1,
	CHUNKTYPE_MESSAGE = 2,
	CHUNKTYPE_DELTA = 3,
};

void CDemoRecorder::WriteTickMarker(int Tick, bool Keyframe)
{
	if(m_LastTickMarker == -1 || Tick - m_LastTickMarker > CHUNKMASK_TICK || Keyframe)
	{
		unsigned char aChunk[sizeof(int32_t) + 1];
		aChunk[0] = CHUNKTYPEFLAG_TICKMARKER;
		uint_to_bytes_be(aChunk + 1, Tick);

		if(Keyframe)
			aChunk[0] |= CHUNKTICKFLAG_KEYFRAME;

		io_write(m_File, aChunk, sizeof(aChunk));
	}
	else
	{
		unsigned char aChunk[1];
		aChunk[0] = CHUNKTYPEFLAG_TICKMARKER | CHUNKTICKFLAG_TICK_COMPRESSED | (Tick - m_LastTickMarker);
		io_write(m_File, aChunk, sizeof(aChunk));
	}

	m_LastTickMarker = Tick;
	if(m_FirstTick < 0)
		m_FirstTick = Tick;
}

void CDemoRecorder::Write(int Type, const void *pData, int Size)
{
	if(!m_File)
		return;

	if(Size > 64 * 1024)
		return;

	/* pad the data with 0 so we get an alignment of 4,
	else the compression won't work and miss some bytes */
	char aBuffer[64 * 1024];
	char aBuffer2[64 * 1024];
	mem_copy(aBuffer2, pData, Size);
	while(Size & 3)
		aBuffer2[Size++] = 0;
	Size = CVariableInt::Compress(aBuffer2, Size, aBuffer, sizeof(aBuffer)); // buffer2 -> buffer
	if(Size < 0)
		return;

	Size = CNetBase::Compress(aBuffer, Size, aBuffer2, sizeof(aBuffer2)); // buffer -> buffer2
	if(Size < 0)
		return;

	unsigned char aChunk[3];
	aChunk[0] = ((Type & 0x3) << 5);
	if(Size < 30)
	{
		aChunk[0] |= Size;
		io_write(m_File, aChunk, 1);
	}
	else
	{
		if(Size < 256)
		{
			aChunk[0] |= 30;
			aChunk[1] = Size & 0xff;
			io_write(m_File, aChunk, 2);
		}
		else
		{
			aChunk[0] |= 31;
			aChunk[1] = Size & 0xff;
			aChunk[2] = Size >> 8;
			io_write(m_File, aChunk, 3);
		}
	}

	io_write(m_File, aBuffer2, Size);
}

void CDemoRecorder::RecordSnapshot(int Tick, const void *pData, int Size)
{
	if(m_LastKeyFrame == -1 || (Tick - m_LastKeyFrame) > SERVER_TICK_SPEED * 5)
	{
		// write full tickmarker
		WriteTickMarker(Tick, true);

		// write snapshot
		Write(CHUNKTYPE_SNAPSHOT, pData, Size);

		m_LastKeyFrame = Tick;
		mem_copy(m_aLastSnapshotData, pData, Size);
	}
	else
	{
		// write tickmarker
		WriteTickMarker(Tick, false);

		// create delta
		char aDeltaData[CSnapshot::MAX_SIZE + sizeof(int)];
		m_pSnapshotDelta->SetStaticsize(protocol7::NETEVENTTYPE_SOUNDWORLD, true);
		m_pSnapshotDelta->SetStaticsize(protocol7::NETEVENTTYPE_DAMAGE, true);
		const int DeltaSize = m_pSnapshotDelta->CreateDelta((CSnapshot *)m_aLastSnapshotData, (CSnapshot *)pData, &aDeltaData);
		if(DeltaSize)
		{
			// record delta
			Write(CHUNKTYPE_DELTA, aDeltaData, DeltaSize);
			mem_copy(m_aLastSnapshotData, pData, Size);
		}
	}
}

void CDemoRecorder::RecordMessage(const void *pData, int Size)
{
	if(m_pfnFilter)
	{
		if(m_pfnFilter(pData, Size, m_pUser))
		{
			return;
		}
	}
	Write(CHUNKTYPE_MESSAGE, pData, Size);
}

int CDemoRecorder::Stop(IDemoRecorder::EStopMode Mode, const char *pTargetFilename)
{
	if(!m_File)
		return -1;

	if(Mode == IDemoRecorder::EStopMode::KEEP_FILE)
	{
		// add the demo length to the header
		io_seek(m_File, offsetof(CDemoHeader, m_aLength), IOSEEK_START);
		unsigned char aLength[sizeof(int32_t)];
		uint_to_bytes_be(aLength, Length());
		io_write(m_File, aLength, sizeof(aLength));

		// add the timeline markers to the header
		io_seek(m_File, sizeof(CDemoHeader) + offsetof(CTimelineMarkers, m_aNumTimelineMarkers), IOSEEK_START);
		unsigned char aNumMarkers[sizeof(int32_t)];
		uint_to_bytes_be(aNumMarkers, m_NumTimelineMarkers);
		io_write(m_File, aNumMarkers, sizeof(aNumMarkers));
		for(int i = 0; i < m_NumTimelineMarkers; i++)
		{
			unsigned char aMarker[sizeof(int32_t)];
			uint_to_bytes_be(aMarker, m_aTimelineMarkers[i]);
			io_write(m_File, aMarker, sizeof(aMarker));
		}
	}

	io_close(m_File);
	m_File = 0;

	if(Mode == IDemoRecorder::EStopMode::REMOVE_FILE)
	{
		if(!m_pStorage->RemoveFile(m_aCurrentFilename, IStorage::TYPE_SAVE))
		{
			if(m_pConsole)
			{
				char aBuf[64 + IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "Could not remove demo file '%s'.", m_aCurrentFilename);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
			}
			return -1;
		}
	}
	else if(pTargetFilename[0] != '\0')
	{
		if(!m_pStorage->RenameFile(m_aCurrentFilename, pTargetFilename, IStorage::TYPE_SAVE))
		{
			if(m_pConsole)
			{
				char aBuf[64 + 2 * IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "Could not move demo file '%s' to '%s'.", m_aCurrentFilename, pTargetFilename);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
			}
			return -1;
		}
	}

	if(m_pConsole)
	{
		char aBuf[64 + IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "Stopped recording to '%s'", m_aCurrentFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
	}

	return 0;
}

void CDemoRecorder::AddDemoMarker()
{
	if(m_LastTickMarker < 0)
		return;
	AddDemoMarker(m_LastTickMarker);
}

void CDemoRecorder::AddDemoMarker(int Tick)
{
	dbg_assert(Tick >= 0, "invalid marker tick");
	if(m_NumTimelineMarkers >= MAX_TIMELINE_MARKERS)
	{
		if(m_pConsole)
		{
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Too many timeline markers", gs_DemoPrintColor);
		}
		return;
	}

	// not more than 1 marker in a second
	if(m_NumTimelineMarkers > 0)
	{
		const int Diff = Tick - m_aTimelineMarkers[m_NumTimelineMarkers - 1];
		if(Diff < (float)SERVER_TICK_SPEED)
		{
			if(m_pConsole)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Previous timeline marker too close", gs_DemoPrintColor);
			}
			return;
		}
	}

	m_aTimelineMarkers[m_NumTimelineMarkers++] = Tick;

	if(m_pConsole)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Added timeline marker", gs_DemoPrintColor);
	}
}

CDemoPlayer::CDemoPlayer(class CSnapshotDelta *pSnapshotDelta, bool UseVideo, TUpdateIntraTimesFunc &&UpdateIntraTimesFunc)
{
	Construct(pSnapshotDelta, UseVideo);

	m_UpdateIntraTimesFunc = UpdateIntraTimesFunc;
}

CDemoPlayer::CDemoPlayer(class CSnapshotDelta *pSnapshotDelta, bool UseVideo)
{
	Construct(pSnapshotDelta, UseVideo);
}

CDemoPlayer::~CDemoPlayer()
{
	dbg_assert(m_File == 0, "Demo player not stopped");
}

void CDemoPlayer::Construct(class CSnapshotDelta *pSnapshotDelta, bool UseVideo)
{
	m_File = 0;
	m_SpeedIndex = 4;

	m_pSnapshotDelta = pSnapshotDelta;
	m_LastSnapshotDataSize = -1;
	m_pListener = nullptr;
	m_UseVideo = UseVideo;

	m_aFilename[0] = '\0';
	m_aErrorMessage[0] = '\0';
}

void CDemoPlayer::SetListener(IListener *pListener)
{
	m_pListener = pListener;
}

CDemoPlayer::EReadChunkHeaderResult CDemoPlayer::ReadChunkHeader(int *pType, int *pSize, int *pTick)
{
	*pSize = 0;
	*pType = 0;

	unsigned char Chunk = 0;
	if(io_read(m_File, &Chunk, sizeof(Chunk)) != sizeof(Chunk))
		return CHUNKHEADER_EOF;

	if(Chunk & CHUNKTYPEFLAG_TICKMARKER)
	{
		// decode tick marker
		int TickdeltaLegacy = Chunk & CHUNKMASK_TICK_LEGACY; // compatibility
		*pType = Chunk & (CHUNKTYPEFLAG_TICKMARKER | CHUNKTICKFLAG_KEYFRAME);

		int NewTick;
		if(m_Info.m_Header.m_Version < gs_VersionTickCompression && TickdeltaLegacy != 0)
		{
			if(*pTick < 0) // initial tick not initialized before a tick delta
				return CHUNKHEADER_ERROR;
			NewTick = *pTick + TickdeltaLegacy;
		}
		else if(Chunk & CHUNKTICKFLAG_TICK_COMPRESSED)
		{
			if(*pTick < 0) // initial tick not initialized before a tick delta
				return CHUNKHEADER_ERROR;
			int Tickdelta = Chunk & CHUNKMASK_TICK;
			NewTick = *pTick + Tickdelta;
		}
		else
		{
			unsigned char aTickdata[sizeof(int32_t)];
			if(io_read(m_File, aTickdata, sizeof(aTickdata)) != sizeof(aTickdata))
				return CHUNKHEADER_ERROR;
			NewTick = bytes_be_to_uint(aTickdata);
		}
		if(NewTick < MIN_TICK || NewTick >= MAX_TICK) // invalid tick
			return CHUNKHEADER_ERROR;
		*pTick = NewTick;
	}
	else
	{
		// decode normal chunk
		*pType = (Chunk & CHUNKMASK_TYPE) >> 5;
		*pSize = Chunk & CHUNKMASK_SIZE;

		if(*pSize == 30)
		{
			unsigned char aSizedata[1];
			if(io_read(m_File, aSizedata, sizeof(aSizedata)) != sizeof(aSizedata))
				return CHUNKHEADER_ERROR;
			*pSize = aSizedata[0];
		}
		else if(*pSize == 31)
		{
			unsigned char aSizedata[2];
			if(io_read(m_File, aSizedata, sizeof(aSizedata)) != sizeof(aSizedata))
				return CHUNKHEADER_ERROR;
			*pSize = (aSizedata[1] << 8) | aSizedata[0];
		}
	}

	return CHUNKHEADER_SUCCESS;
}

bool CDemoPlayer::ScanFile()
{
	const int64_t StartPos = io_tell(m_File);
	m_vKeyFrames.clear();
	if(StartPos < 0)
		return false;

	int ChunkTick = -1;
	while(true)
	{
		const int64_t CurrentPos = io_tell(m_File);
		if(CurrentPos < 0)
		{
			m_vKeyFrames.clear();
			return false;
		}

		int ChunkType, ChunkSize;
		const EReadChunkHeaderResult Result = ReadChunkHeader(&ChunkType, &ChunkSize, &ChunkTick);
		if(Result == CHUNKHEADER_EOF)
		{
			break;
		}
		else if(Result == CHUNKHEADER_ERROR)
		{
			m_vKeyFrames.clear();
			return false;
		}

		if(ChunkType & CHUNKTYPEFLAG_TICKMARKER)
		{
			if(ChunkType & CHUNKTICKFLAG_KEYFRAME)
			{
				m_vKeyFrames.emplace_back(CurrentPos, ChunkTick);
			}

			if(m_Info.m_Info.m_FirstTick == -1)
				m_Info.m_Info.m_FirstTick = ChunkTick;
			m_Info.m_Info.m_LastTick = ChunkTick;
		}
		else if(ChunkSize)
		{
			if(io_skip(m_File, ChunkSize) != 0)
			{
				m_vKeyFrames.clear();
				return false;
			}
		}
	}

	if(io_seek(m_File, StartPos, IOSEEK_START) != 0)
	{
		m_vKeyFrames.clear();
		return false;
	}
	return true;
}

void CDemoPlayer::DoTick()
{
	// update ticks
	m_Info.m_PreviousTick = m_Info.m_Info.m_CurrentTick;
	m_Info.m_Info.m_CurrentTick = m_Info.m_NextTick;
	int ChunkTick = m_Info.m_Info.m_CurrentTick;

	int64_t Freq = time_freq();
	int64_t CurtickStart = m_Info.m_Info.m_CurrentTick * Freq / SERVER_TICK_SPEED;
	int64_t PrevtickStart = m_Info.m_PreviousTick * Freq / SERVER_TICK_SPEED;
	m_Info.m_IntraTick = (m_Info.m_CurrentTime - PrevtickStart) / (float)(CurtickStart - PrevtickStart);
	m_Info.m_IntraTickSincePrev = (m_Info.m_CurrentTime - PrevtickStart) / (float)(Freq / SERVER_TICK_SPEED);
	m_Info.m_TickTime = (m_Info.m_CurrentTime - PrevtickStart) / (float)Freq;
	if(m_UpdateIntraTimesFunc)
		m_UpdateIntraTimesFunc();

	bool GotSnapshot = false;
	while(true)
	{
		int ChunkType, ChunkSize;
		const EReadChunkHeaderResult Result = ReadChunkHeader(&ChunkType, &ChunkSize, &ChunkTick);
		if(Result == CHUNKHEADER_EOF)
		{
			if(m_Info.m_PreviousTick == -1)
			{
				Stop("Empty demo");
			}
			else
			{
				Pause();
				// Stop rendering when reaching end of file
#if defined(CONF_VIDEORECORDER)
				if(m_UseVideo && IVideo::Current())
					Stop();
#endif
			}
			break;
		}
		else if(Result == CHUNKHEADER_ERROR)
		{
			Stop("Error reading chunk header");
			break;
		}

		// read the chunk
		int DataSize = 0;
		if(ChunkSize)
		{
			if(io_read(m_File, m_aCompressedSnapshotData, ChunkSize) != (unsigned)ChunkSize)
			{
				Stop("Error reading chunk data");
				break;
			}

			DataSize = CNetBase::Decompress(m_aCompressedSnapshotData, ChunkSize, m_aDecompressedSnapshotData, sizeof(m_aDecompressedSnapshotData));
			if(DataSize < 0)
			{
				Stop("Error during network decompression");
				break;
			}

			DataSize = CVariableInt::Decompress(m_aDecompressedSnapshotData, DataSize, m_aChunkData, sizeof(m_aChunkData));
			if(DataSize < 0)
			{
				Stop("Error during intpack decompression");
				break;
			}
		}

		if(ChunkType == CHUNKTYPE_DELTA)
		{
			// process delta snapshot
			CSnapshot *pNewsnap = (CSnapshot *)m_aSnapshot;
			DataSize = m_pSnapshotDelta->UnpackDelta((CSnapshot *)m_aLastSnapshotData, pNewsnap, m_aChunkData, DataSize, IsSixup());

			if(DataSize < 0)
			{
				if(m_pConsole)
				{
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Error unpacking snapshot delta. DataSize=%d", DataSize);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
				}
			}
			else if(!pNewsnap->IsValid(DataSize))
			{
				if(m_pConsole)
				{
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Snapshot delta invalid. DataSize=%d", DataSize);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
				}
			}
			else
			{
				if(m_pListener)
					m_pListener->OnDemoPlayerSnapshot(m_aSnapshot, DataSize);

				m_LastSnapshotDataSize = DataSize;
				mem_copy(m_aLastSnapshotData, m_aSnapshot, DataSize);
				GotSnapshot = true;
			}
		}
		else if(ChunkType == CHUNKTYPE_SNAPSHOT)
		{
			// process full snapshot
			CSnapshot *pSnap = (CSnapshot *)m_aChunkData;
			if(!pSnap->IsValid(DataSize))
			{
				if(m_pConsole)
				{
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Snapshot invalid. DataSize=%d", DataSize);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
				}
			}
			else
			{
				GotSnapshot = true;

				m_LastSnapshotDataSize = DataSize;
				mem_copy(m_aLastSnapshotData, m_aChunkData, DataSize);
				if(m_pListener)
					m_pListener->OnDemoPlayerSnapshot(m_aChunkData, DataSize);
			}
		}
		else
		{
			// if there were no snapshots in this tick, replay the last one
			if(!GotSnapshot && m_pListener && m_LastSnapshotDataSize != -1)
			{
				GotSnapshot = true;
				m_pListener->OnDemoPlayerSnapshot(m_aLastSnapshotData, m_LastSnapshotDataSize);
			}

			// check the remaining types
			if(ChunkType & CHUNKTYPEFLAG_TICKMARKER)
			{
				m_Info.m_NextTick = ChunkTick;
				break;
			}
			else if(ChunkType == CHUNKTYPE_MESSAGE)
			{
				if(m_pListener)
					m_pListener->OnDemoPlayerMessage(m_aChunkData, DataSize);
			}
		}
	}
}

void CDemoPlayer::Pause()
{
	m_Info.m_Info.m_Paused = true;
#if defined(CONF_VIDEORECORDER)
	if(m_UseVideo && IVideo::Current() && g_Config.m_ClVideoPauseWithDemo)
		IVideo::Current()->Pause(true);
#endif
}

void CDemoPlayer::Unpause()
{
	m_Info.m_Info.m_Paused = false;
#if defined(CONF_VIDEORECORDER)
	if(m_UseVideo && IVideo::Current() && g_Config.m_ClVideoPauseWithDemo)
		IVideo::Current()->Pause(false);
#endif
}

int CDemoPlayer::Load(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, int StorageType)
{
	dbg_assert(m_File == 0, "Demo player already playing");

	m_pConsole = pConsole;
	str_copy(m_aFilename, pFilename);
	str_copy(m_aErrorMessage, "");

	if(m_pConsole)
	{
		char aBuf[32 + IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "Loading demo '%s'", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", aBuf);
	}

	// clear the playback info
	mem_zero(&m_Info, sizeof(m_Info));
	m_Info.m_Info.m_FirstTick = -1;
	m_Info.m_Info.m_LastTick = -1;
	m_Info.m_NextTick = -1;
	m_Info.m_Info.m_CurrentTick = -1;
	m_Info.m_PreviousTick = -1;
	m_Info.m_Info.m_Speed = 1;
	m_SpeedIndex = 4;
	m_LastSnapshotDataSize = -1;

	if(!GetDemoInfo(pStorage, m_pConsole, pFilename, StorageType, &m_Info.m_Header, &m_Info.m_TimelineMarkers, &m_MapInfo, &m_File, m_aErrorMessage, sizeof(m_aErrorMessage)))
	{
		str_copy(m_aFilename, "");
		return -1;
	}
	m_Sixup = str_startswith(m_Info.m_Header.m_aNetversion, "0.7");

	// save byte offset of map for later use
	m_MapOffset = io_tell(m_File);
	if(m_MapOffset < 0 || io_skip(m_File, m_MapInfo.m_Size) != 0)
	{
		Stop("Error skipping map data");
		return -1;
	}

	if(m_Info.m_Header.m_Version > gs_OldVersion)
	{
		// get timeline markers
		int Num = bytes_be_to_uint(m_Info.m_TimelineMarkers.m_aNumTimelineMarkers);
		m_Info.m_Info.m_NumTimelineMarkers = clamp<int>(Num, 0, MAX_TIMELINE_MARKERS);
		for(int i = 0; i < m_Info.m_Info.m_NumTimelineMarkers; i++)
		{
			m_Info.m_Info.m_aTimelineMarkers[i] = bytes_be_to_uint(m_Info.m_TimelineMarkers.m_aTimelineMarkers[i]);
		}
	}

	// scan the file for interesting points
	if(!ScanFile())
	{
		Stop("Error scanning demo file");
		return -1;
	}

	// reset slice markers
	g_Config.m_ClDemoSliceBegin = -1;
	g_Config.m_ClDemoSliceEnd = -1;

	// ready for playback
	return 0;
}

unsigned char *CDemoPlayer::GetMapData(class IStorage *pStorage)
{
	if(!m_MapInfo.m_Size)
		return nullptr;

	const int64_t CurSeek = io_tell(m_File);
	if(CurSeek < 0 || io_seek(m_File, m_MapOffset, IOSEEK_START) != 0)
		return nullptr;
	unsigned char *pMapData = (unsigned char *)malloc(m_MapInfo.m_Size);
	if(io_read(m_File, pMapData, m_MapInfo.m_Size) != m_MapInfo.m_Size ||
		io_seek(m_File, CurSeek, IOSEEK_START) != 0)
	{
		free(pMapData);
		return nullptr;
	}
	return pMapData;
}

bool CDemoPlayer::ExtractMap(class IStorage *pStorage)
{
	unsigned char *pMapData = GetMapData(pStorage);
	if(!pMapData)
		return false;

	// handle sha256
	SHA256_DIGEST Sha256 = SHA256_ZEROED;
	if(m_Info.m_Header.m_Version >= gs_Sha256Version)
		Sha256 = m_MapInfo.m_Sha256;
	else
	{
		Sha256 = sha256(pMapData, m_MapInfo.m_Size);
		m_MapInfo.m_Sha256 = Sha256;
	}

	// construct name
	char aSha[SHA256_MAXSTRSIZE], aMapFilename[IO_MAX_PATH_LENGTH];
	sha256_str(Sha256, aSha, sizeof(aSha));
	str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s_%s.map", m_Info.m_Header.m_aMapName, aSha);

	// save map
	IOHANDLE MapFile = pStorage->OpenFile(aMapFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!MapFile)
	{
		free(pMapData);
		return false;
	}

	io_write(MapFile, pMapData, m_MapInfo.m_Size);
	io_close(MapFile);

	// free data
	free(pMapData);
	return true;
}

int64_t CDemoPlayer::Time()
{
#if defined(CONF_VIDEORECORDER)
	if(m_UseVideo && IVideo::Current())
	{
		if(!m_WasRecording)
		{
			m_WasRecording = true;
			m_Info.m_LastUpdate = IVideo::Time();
		}
		return IVideo::Time();
	}
	else
	{
		const int64_t Now = time_get();
		if(m_WasRecording)
		{
			m_WasRecording = false;
			m_Info.m_LastUpdate = Now;
		}
		return Now;
	}
#else
	return time_get();
#endif
}

int CDemoPlayer::Play()
{
	// fill in previous and next tick
	while(m_Info.m_PreviousTick == -1 && IsPlaying())
		DoTick();

	// set start info
	m_Info.m_CurrentTime = m_Info.m_PreviousTick * time_freq() / SERVER_TICK_SPEED;
	m_Info.m_LastUpdate = Time();
	return 0;
}

int CDemoPlayer::SeekPercent(float Percent)
{
	int WantedTick = m_Info.m_Info.m_FirstTick + round_truncate((m_Info.m_Info.m_LastTick - m_Info.m_Info.m_FirstTick) * Percent);
	return SetPos(WantedTick);
}

int CDemoPlayer::SeekTime(float Seconds)
{
	int WantedTick = m_Info.m_Info.m_CurrentTick + round_truncate(Seconds * (float)SERVER_TICK_SPEED);
	return SetPos(WantedTick);
}

int CDemoPlayer::SeekTick(ETickOffset TickOffset)
{
	int WantedTick;
	switch(TickOffset)
	{
	case TICK_CURRENT:
		WantedTick = m_Info.m_Info.m_CurrentTick;
		break;
	case TICK_PREVIOUS:
		WantedTick = m_Info.m_PreviousTick;
		break;
	case TICK_NEXT:
		WantedTick = m_Info.m_NextTick;
		break;
	default:
		dbg_assert(false, "Invalid TickOffset");
		WantedTick = -1;
		break;
	}

	// +1 because SetPos will seek until the given tick is the next tick that
	// will be played back, whereas we want the wanted tick to be played now.
	return SetPos(WantedTick + 1);
}

int CDemoPlayer::SetPos(int WantedTick)
{
	if(!m_File)
		return -1;

	WantedTick = clamp(WantedTick, m_Info.m_Info.m_FirstTick, m_Info.m_Info.m_LastTick);
	const int KeyFrameWantedTick = WantedTick - 5; // -5 because we have to have a current tick and previous tick when we do the playback
	const float Percent = (KeyFrameWantedTick - m_Info.m_Info.m_FirstTick) / (float)(m_Info.m_Info.m_LastTick - m_Info.m_Info.m_FirstTick);

	// get correct key frame
	size_t KeyFrame = clamp<size_t>(m_vKeyFrames.size() * Percent, 0, m_vKeyFrames.size() - 1);
	while(KeyFrame < m_vKeyFrames.size() - 1 && m_vKeyFrames[KeyFrame].m_Tick < KeyFrameWantedTick)
		KeyFrame++;
	while(KeyFrame > 0 && m_vKeyFrames[KeyFrame].m_Tick > KeyFrameWantedTick)
		KeyFrame--;

	// seek to the correct key frame
	if(io_seek(m_File, m_vKeyFrames[KeyFrame].m_Filepos, IOSEEK_START) != 0)
	{
		Stop("Error seeking keyframe position");
		return -1;
	}

	m_Info.m_NextTick = -1;
	m_Info.m_Info.m_CurrentTick = -1;
	m_Info.m_PreviousTick = -1;

	// playback everything until we hit our tick
	while(m_Info.m_NextTick < WantedTick && IsPlaying())
		DoTick();

	Play();

	return 0;
}

void CDemoPlayer::SetSpeed(float Speed)
{
	m_Info.m_Info.m_Speed = clamp(Speed, 0.f, 256.f);
}

void CDemoPlayer::SetSpeedIndex(int SpeedIndex)
{
	dbg_assert(SpeedIndex >= 0 && SpeedIndex < g_DemoSpeeds, "invalid SpeedIndex");
	m_SpeedIndex = SpeedIndex;
	SetSpeed(g_aSpeeds[m_SpeedIndex]);
}

void CDemoPlayer::AdjustSpeedIndex(int Offset)
{
	SetSpeedIndex(clamp(m_SpeedIndex + Offset, 0, (int)(std::size(g_aSpeeds) - 1)));
}

int CDemoPlayer::Update(bool RealTime)
{
	int64_t Now = Time();
	int64_t Deltatime = Now - m_Info.m_LastUpdate;
	m_Info.m_LastUpdate = Now;

	if(!IsPlaying())
		return 0;

	const int64_t Freq = time_freq();
	if(!m_Info.m_Info.m_Paused)
	{
		m_Info.m_CurrentTime += (int64_t)(Deltatime * (double)m_Info.m_Info.m_Speed);

		while(!m_Info.m_Info.m_Paused && IsPlaying())
		{
			int64_t CurtickStart = m_Info.m_Info.m_CurrentTick * Freq / SERVER_TICK_SPEED;

			// break if we are ready
			if(RealTime && CurtickStart > m_Info.m_CurrentTime)
				break;

			// do one more tick
			DoTick();
		}
	}

	// update intratick
	{
		int64_t CurtickStart = m_Info.m_Info.m_CurrentTick * Freq / SERVER_TICK_SPEED;
		int64_t PrevtickStart = m_Info.m_PreviousTick * Freq / SERVER_TICK_SPEED;
		m_Info.m_IntraTick = (m_Info.m_CurrentTime - PrevtickStart) / (float)(CurtickStart - PrevtickStart);
		m_Info.m_IntraTickSincePrev = (m_Info.m_CurrentTime - PrevtickStart) / (float)(Freq / SERVER_TICK_SPEED);
		m_Info.m_TickTime = (m_Info.m_CurrentTime - PrevtickStart) / (float)Freq;
		if(m_UpdateIntraTimesFunc)
			m_UpdateIntraTimesFunc();
	}

	return 0;
}

void CDemoPlayer::Stop(const char *pErrorMessage)
{
#if defined(CONF_VIDEORECORDER)
	if(m_UseVideo && IVideo::Current())
		IVideo::Current()->Stop();
#endif

	if(!m_File)
		return;

	if(m_pConsole)
	{
		char aBuf[256];
		if(pErrorMessage[0] == '\0')
			str_copy(aBuf, "Stopped playback");
		else
			str_format(aBuf, sizeof(aBuf), "Stopped playback due to error: %s", pErrorMessage);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", aBuf);
	}

	io_close(m_File);
	m_File = 0;
	m_vKeyFrames.clear();
	str_copy(m_aFilename, "");
	str_copy(m_aErrorMessage, pErrorMessage);
}

void CDemoPlayer::GetDemoName(char *pBuffer, size_t BufferSize) const
{
	IStorage::StripPathAndExtension(m_aFilename, pBuffer, BufferSize);
}

bool CDemoPlayer::GetDemoInfo(IStorage *pStorage, IConsole *pConsole, const char *pFilename, int StorageType, CDemoHeader *pDemoHeader, CTimelineMarkers *pTimelineMarkers, CMapInfo *pMapInfo, IOHANDLE *pFile, char *pErrorMessage, size_t ErrorMessageSize) const
{
	mem_zero(pDemoHeader, sizeof(CDemoHeader));
	mem_zero(pTimelineMarkers, sizeof(CTimelineMarkers));
	mem_zero(pMapInfo, sizeof(CMapInfo));

	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType);
	if(!File)
	{
		if(pErrorMessage != nullptr)
			str_copy(pErrorMessage, "Could not open demo file", ErrorMessageSize);
		return false;
	}

	if(io_read(File, pDemoHeader, sizeof(CDemoHeader)) != sizeof(CDemoHeader) || !pDemoHeader->Valid())
	{
		if(pErrorMessage != nullptr)
			str_copy(pErrorMessage, "Error reading demo header", ErrorMessageSize);
		mem_zero(pDemoHeader, sizeof(CDemoHeader));
		io_close(File);
		return false;
	}

	if(pDemoHeader->m_Version < gs_OldVersion)
	{
		if(pErrorMessage != nullptr)
			str_format(pErrorMessage, ErrorMessageSize, "Demo version '%d' is not supported", pDemoHeader->m_Version);
		mem_zero(pDemoHeader, sizeof(CDemoHeader));
		io_close(File);
		return false;
	}
	else if(pDemoHeader->m_Version > gs_OldVersion)
	{
		if(io_read(File, pTimelineMarkers, sizeof(CTimelineMarkers)) != sizeof(CTimelineMarkers))
		{
			if(pErrorMessage != nullptr)
				str_copy(pErrorMessage, "Error reading timeline markers", ErrorMessageSize);
			mem_zero(pDemoHeader, sizeof(CDemoHeader));
			io_close(File);
			return false;
		}
	}

	SHA256_DIGEST Sha256 = SHA256_ZEROED;
	if(pDemoHeader->m_Version >= gs_Sha256Version)
	{
		CUuid ExtensionUuid = {};
		const unsigned ExtensionUuidSize = io_read(File, &ExtensionUuid.m_aData, sizeof(ExtensionUuid.m_aData));
		if(ExtensionUuidSize == sizeof(ExtensionUuid.m_aData) && ExtensionUuid == SHA256_EXTENSION)
		{
			if(io_read(File, &Sha256, sizeof(SHA256_DIGEST)) != sizeof(SHA256_DIGEST))
			{
				if(pErrorMessage != nullptr)
					str_copy(pErrorMessage, "Error reading SHA256", ErrorMessageSize);
				mem_zero(pDemoHeader, sizeof(CDemoHeader));
				mem_zero(pTimelineMarkers, sizeof(CTimelineMarkers));
				io_close(File);
				return false;
			}
		}
		else
		{
			// This hopes whatever happened during the version increment didn't add something here
			if(pConsole)
			{
				pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", "Demo version incremented, but not by DDNet");
			}
			if(io_seek(File, -(int64_t)ExtensionUuidSize, IOSEEK_CUR) != 0)
			{
				if(pErrorMessage != nullptr)
					str_copy(pErrorMessage, "Error rewinding SHA256 extension UUID", ErrorMessageSize);
				mem_zero(pDemoHeader, sizeof(CDemoHeader));
				mem_zero(pTimelineMarkers, sizeof(CTimelineMarkers));
				io_close(File);
				return false;
			}
		}
	}

	str_copy(pMapInfo->m_aName, pDemoHeader->m_aMapName);
	pMapInfo->m_Sha256 = Sha256;
	pMapInfo->m_Crc = bytes_be_to_uint(pDemoHeader->m_aMapCrc);
	pMapInfo->m_Size = bytes_be_to_uint(pDemoHeader->m_aMapSize);

	if(pFile == nullptr)
		io_close(File);
	else
		*pFile = File;

	return true;
}

class CDemoRecordingListener : public CDemoPlayer::IListener
{
public:
	CDemoRecorder *m_pDemoRecorder;
	CDemoPlayer *m_pDemoPlayer;
	bool m_Stop;
	int m_StartTick;
	int m_EndTick;

	void OnDemoPlayerSnapshot(void *pData, int Size) override
	{
		const CDemoPlayer::CPlaybackInfo *pInfo = m_pDemoPlayer->Info();

		if(m_EndTick != -1 && pInfo->m_Info.m_CurrentTick > m_EndTick)
			m_Stop = true;
		else if(m_StartTick == -1 || pInfo->m_Info.m_CurrentTick >= m_StartTick)
			m_pDemoRecorder->RecordSnapshot(pInfo->m_Info.m_CurrentTick, pData, Size);
	}

	void OnDemoPlayerMessage(void *pData, int Size) override
	{
		const CDemoPlayer::CPlaybackInfo *pInfo = m_pDemoPlayer->Info();

		if(m_EndTick != -1 && pInfo->m_Info.m_CurrentTick > m_EndTick)
			m_Stop = true;
		else if(m_StartTick == -1 || pInfo->m_Info.m_CurrentTick >= m_StartTick)
			m_pDemoRecorder->RecordMessage(pData, Size);
	}
};

void CDemoEditor::Init(class CSnapshotDelta *pSnapshotDelta, class IConsole *pConsole, class IStorage *pStorage)
{
	m_pSnapshotDelta = pSnapshotDelta;
	m_pConsole = pConsole;
	m_pStorage = pStorage;
}

bool CDemoEditor::Slice(const char *pDemo, const char *pDst, int StartTick, int EndTick, DEMOFUNC_FILTER pfnFilter, void *pUser)
{
	CDemoPlayer DemoPlayer(m_pSnapshotDelta, false);
	if(DemoPlayer.Load(m_pStorage, m_pConsole, pDemo, IStorage::TYPE_ALL_OR_ABSOLUTE) == -1)
		return false;

	const CMapInfo *pMapInfo = DemoPlayer.GetMapInfo();
	const CDemoPlayer::CPlaybackInfo *pInfo = DemoPlayer.Info();

	SHA256_DIGEST Sha256 = pMapInfo->m_Sha256;
	if(pInfo->m_Header.m_Version < gs_Sha256Version)
	{
		if(DemoPlayer.ExtractMap(m_pStorage))
			Sha256 = pMapInfo->m_Sha256;
	}

	CDemoRecorder DemoRecorder(m_pSnapshotDelta);
	unsigned char *pMapData = DemoPlayer.GetMapData(m_pStorage);
	const int Result = DemoRecorder.Start(m_pStorage, m_pConsole, pDst, pInfo->m_Header.m_aNetversion, pMapInfo->m_aName, Sha256, pMapInfo->m_Crc, pInfo->m_Header.m_aType, pMapInfo->m_Size, pMapData, nullptr, pfnFilter, pUser) == -1;
	free(pMapData);
	if(Result != 0)
	{
		DemoPlayer.Stop();
		return false;
	}

	CDemoRecordingListener Listener;
	Listener.m_pDemoRecorder = &DemoRecorder;
	Listener.m_pDemoPlayer = &DemoPlayer;
	Listener.m_Stop = false;
	Listener.m_StartTick = StartTick;
	Listener.m_EndTick = EndTick;
	DemoPlayer.SetListener(&Listener);

	DemoPlayer.Play();

	while(DemoPlayer.IsPlaying() && !Listener.m_Stop)
	{
		DemoPlayer.Update(false);

		if(pInfo->m_Info.m_Paused)
			break;
	}

	// Copy timeline markers to sliced demo
	for(int i = 0; i < pInfo->m_Info.m_NumTimelineMarkers; i++)
	{
		if((StartTick == -1 || pInfo->m_Info.m_aTimelineMarkers[i] >= StartTick) && (EndTick == -1 || pInfo->m_Info.m_aTimelineMarkers[i] <= EndTick))
		{
			DemoRecorder.AddDemoMarker(pInfo->m_Info.m_aTimelineMarkers[i]);
		}
	}

	DemoPlayer.Stop();
	DemoRecorder.Stop(IDemoRecorder::EStopMode::KEEP_FILE);
	return true;
}
