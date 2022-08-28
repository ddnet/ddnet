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
#include "memheap.h"
#include "network.h"
#include "snapshot.h"

const double g_aSpeeds[g_DemoSpeeds] = {0.1, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 20.0, 24.0, 28.0, 32.0, 40.0, 48.0, 56.0, 64.0};
const CUuid SHA256_EXTENSION =
	{{0x6b, 0xe6, 0xda, 0x4a, 0xce, 0xbd, 0x38, 0x0c,
		0x9b, 0x5b, 0x12, 0x89, 0xc8, 0x42, 0xd7, 0x80}};

static const unsigned char gs_aHeaderMarker[7] = {'T', 'W', 'D', 'E', 'M', 'O', 0};
static const unsigned char gs_CurVersion = 6;
static const unsigned char gs_OldVersion = 3;
static const unsigned char gs_Sha256Version = 6;
static const unsigned char gs_VersionTickCompression = 5; // demo files with this version or higher will use `CHUNKTICKFLAG_TICK_COMPRESSED`
static const int gs_LengthOffset = 152;
static const int gs_NumMarkersOffset = 176;

static const ColorRGBA gs_DemoPrintColor{0.75f, 0.7f, 0.7f, 1.0f};

CDemoRecorder::CDemoRecorder(class CSnapshotDelta *pSnapshotDelta, bool NoMapData)
{
	m_File = 0;
	m_aCurrentFilename[0] = '\0';
	m_pfnFilter = 0;
	m_pUser = 0;
	m_LastTickMarker = -1;
	m_pSnapshotDelta = pSnapshotDelta;
	m_NoMapData = NoMapData;
}

// Record
int CDemoRecorder::Start(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, const char *pNetVersion, const char *pMap, SHA256_DIGEST *pSha256, unsigned Crc, const char *pType, unsigned MapSize, unsigned char *pMapData, IOHANDLE MapFile, DEMOFUNC_FILTER pfnFilter, void *pUser)
{
	m_pfnFilter = pfnFilter;
	m_pUser = pUser;

	m_pMapData = pMapData;
	m_pConsole = pConsole;

	IOHANDLE DemoFile = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!DemoFile)
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Unable to open '%s' for recording", pFilename);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
		}
		return -1;
	}

	CDemoHeader Header;
	CTimelineMarkers TimelineMarkers;
	if(m_File)
	{
		io_close(DemoFile);
		return -1;
	}

	bool CloseMapFile = false;

	if(MapFile)
		io_seek(MapFile, 0, IOSEEK_START);

	char aSha256[SHA256_MAXSTRSIZE];
	if(pSha256)
		sha256_str(*pSha256, aSha256, sizeof(aSha256));

	if(!pMapData && !MapFile)
	{
		// open mapfile
		char aMapFilename[128];
		// try the downloaded maps
		if(pSha256)
		{
			str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s_%s.map", pMap, aSha256);
		}
		else
		{
			str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s_%08x.map", pMap, Crc);
		}
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
			char aBuf[512];
			str_format(aMapFilename, sizeof(aMapFilename), "%s.map", pMap);
			if(pStorage->FindFile(aMapFilename, "maps", IStorage::TYPE_ALL, aBuf, sizeof(aBuf)))
				MapFile = pStorage->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		}
		if(!MapFile)
		{
			if(m_pConsole)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Unable to open mapfile '%s'", pMap);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
			}
			return -1;
		}

		CloseMapFile = true;
	}

	if(m_NoMapData)
		MapSize = 0;
	else if(MapFile)
		MapSize = io_length(MapFile);

	// write header
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
	io_write(DemoFile, &TimelineMarkers, sizeof(TimelineMarkers)); // fill this on stop

	//Write Sha256
	io_write(DemoFile, SHA256_EXTENSION.m_aData, sizeof(SHA256_EXTENSION.m_aData));
	io_write(DemoFile, pSha256, sizeof(SHA256_DIGEST));

	if(m_NoMapData)
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
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Recording to '%s'", pFilename);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", aBuf, gs_DemoPrintColor);
	}
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

	CHUNKFLAG_BIGSIZE = 0x10
};

void CDemoRecorder::WriteTickMarker(int Tick, int Keyframe)
{
	if(m_LastTickMarker == -1 || Tick - m_LastTickMarker > CHUNKMASK_TICK || Keyframe)
	{
		unsigned char aChunk[5];
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
		WriteTickMarker(Tick, 1);

		// write snapshot
		Write(CHUNKTYPE_SNAPSHOT, pData, Size);

		m_LastKeyFrame = Tick;
		mem_copy(m_aLastSnapshotData, pData, Size);
	}
	else
	{
		// create delta, prepend tick
		char aDeltaData[CSnapshot::MAX_SIZE + sizeof(int)];
		int DeltaSize;

		// write tickmarker
		WriteTickMarker(Tick, 0);

		DeltaSize = m_pSnapshotDelta->CreateDelta((CSnapshot *)m_aLastSnapshotData, (CSnapshot *)pData, &aDeltaData);
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

int CDemoRecorder::Stop()
{
	if(!m_File)
		return -1;

	// add the demo length to the header
	io_seek(m_File, gs_LengthOffset, IOSEEK_START);
	unsigned char aLength[4];
	int_to_bytes_be(aLength, Length());
	io_write(m_File, aLength, sizeof(aLength));

	// add the timeline markers to the header
	io_seek(m_File, gs_NumMarkersOffset, IOSEEK_START);
	unsigned char aNumMarkers[4];
	int_to_bytes_be(aNumMarkers, m_NumTimelineMarkers);
	io_write(m_File, aNumMarkers, sizeof(aNumMarkers));
	for(int i = 0; i < m_NumTimelineMarkers; i++)
	{
		unsigned char aMarker[4];
		int_to_bytes_be(aMarker, m_aTimelineMarkers[i]);
		io_write(m_File, aMarker, sizeof(aMarker));
	}

	io_close(m_File);
	m_File = 0;
	if(m_pConsole)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Stopped recording", gs_DemoPrintColor);

	return 0;
}

void CDemoRecorder::AddDemoMarker()
{
	if(m_LastTickMarker < 0 || m_NumTimelineMarkers >= MAX_TIMELINE_MARKERS)
		return;

	// not more than 1 marker in a second
	if(m_NumTimelineMarkers > 0)
	{
		int Diff = m_LastTickMarker - m_aTimelineMarkers[m_NumTimelineMarkers - 1];
		if(Diff < (float)SERVER_TICK_SPEED)
			return;
	}

	m_aTimelineMarkers[m_NumTimelineMarkers++] = m_LastTickMarker;

	if(m_pConsole)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Added timeline marker", gs_DemoPrintColor);
}

CDemoPlayer::CDemoPlayer(class CSnapshotDelta *pSnapshotDelta, TUpdateIntraTimesFunc &&UpdateIntraTimesFunc)
{
	Construct(pSnapshotDelta);

	m_UpdateIntraTimesFunc = UpdateIntraTimesFunc;
}

CDemoPlayer::CDemoPlayer(class CSnapshotDelta *pSnapshotDelta)
{
	Construct(pSnapshotDelta);
}

void CDemoPlayer::Construct(class CSnapshotDelta *pSnapshotDelta)
{
	m_File = 0;
	m_pKeyFrames = 0;
	m_SpeedIndex = 4;

	m_pSnapshotDelta = pSnapshotDelta;
	m_LastSnapshotDataSize = -1;
}

void CDemoPlayer::SetListener(IListener *pListener)
{
	m_pListener = pListener;
}

int CDemoPlayer::ReadChunkHeader(int *pType, int *pSize, int *pTick)
{
	*pSize = 0;
	*pType = 0;

	if(m_File == NULL)
		return -1;

	unsigned char Chunk = 0;
	if(io_read(m_File, &Chunk, sizeof(Chunk)) != sizeof(Chunk))
		return -1;

	if(Chunk & CHUNKTYPEFLAG_TICKMARKER)
	{
		// decode tick marker
		int Tickdelta_legacy = Chunk & (CHUNKMASK_TICK_LEGACY); // compatibility
		*pType = Chunk & (CHUNKTYPEFLAG_TICKMARKER | CHUNKTICKFLAG_KEYFRAME);

		if(m_Info.m_Header.m_Version < gs_VersionTickCompression && Tickdelta_legacy != 0)
		{
			*pTick += Tickdelta_legacy;
		}
		else if(Chunk & (CHUNKTICKFLAG_TICK_COMPRESSED))
		{
			int Tickdelta = Chunk & (CHUNKMASK_TICK);
			*pTick += Tickdelta;
		}
		else
		{
			unsigned char aTickdata[4];
			if(io_read(m_File, aTickdata, sizeof(aTickdata)) != sizeof(aTickdata))
				return -1;
			*pTick = bytes_be_to_int(aTickdata);
		}
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
				return -1;
			*pSize = aSizedata[0];
		}
		else if(*pSize == 31)
		{
			unsigned char aSizedata[2];
			if(io_read(m_File, aSizedata, sizeof(aSizedata)) != sizeof(aSizedata))
				return -1;
			*pSize = (aSizedata[1] << 8) | aSizedata[0];
		}
	}

	return 0;
}

void CDemoPlayer::ScanFile()
{
	CHeap Heap;
	CKeyFrameSearch *pFirstKey = 0;
	CKeyFrameSearch *pCurrentKey = 0;
	int ChunkTick = 0;

	long StartPos = io_tell(m_File);
	m_Info.m_SeekablePoints = 0;

	while(true)
	{
		long CurrentPos = io_tell(m_File);

		int ChunkSize, ChunkType;
		if(ReadChunkHeader(&ChunkType, &ChunkSize, &ChunkTick))
			break;

		// read the chunk
		if(ChunkType & CHUNKTYPEFLAG_TICKMARKER)
		{
			if(ChunkType & CHUNKTICKFLAG_KEYFRAME)
			{
				CKeyFrameSearch *pKey;

				// save the position
				pKey = (CKeyFrameSearch *)Heap.Allocate(sizeof(CKeyFrameSearch));
				pKey->m_Frame.m_Filepos = CurrentPos;
				pKey->m_Frame.m_Tick = ChunkTick;
				pKey->m_pNext = 0;
				if(pCurrentKey)
					pCurrentKey->m_pNext = pKey;
				if(!pFirstKey)
					pFirstKey = pKey;
				pCurrentKey = pKey;
				m_Info.m_SeekablePoints++;
			}

			if(m_Info.m_Info.m_FirstTick == -1)
				m_Info.m_Info.m_FirstTick = ChunkTick;
			m_Info.m_Info.m_LastTick = ChunkTick;
		}
		else if(ChunkSize)
			io_skip(m_File, ChunkSize);
	}

	// copy all the frames to an array instead for fast access
	int i;
	m_pKeyFrames = (CKeyFrame *)calloc(maximum(m_Info.m_SeekablePoints, 1), sizeof(CKeyFrame));
	for(pCurrentKey = pFirstKey, i = 0; pCurrentKey; pCurrentKey = pCurrentKey->m_pNext, i++)
		m_pKeyFrames[i] = pCurrentKey->m_Frame;

	// destroy the temporary heap and seek back to the start
	io_seek(m_File, StartPos, IOSEEK_START);
}

void CDemoPlayer::DoTick()
{
	// update ticks
	m_Info.m_PreviousTick = m_Info.m_Info.m_CurrentTick;
	m_Info.m_Info.m_CurrentTick = m_Info.m_NextTick;
	int ChunkTick = m_Info.m_Info.m_CurrentTick;

	int64_t Freq = time_freq();
	int64_t CurtickStart = (m_Info.m_Info.m_CurrentTick) * Freq / SERVER_TICK_SPEED;
	int64_t PrevtickStart = (m_Info.m_PreviousTick) * Freq / SERVER_TICK_SPEED;
	m_Info.m_IntraTick = (m_Info.m_CurrentTime - PrevtickStart) / (float)(CurtickStart - PrevtickStart);
	m_Info.m_IntraTickSincePrev = (m_Info.m_CurrentTime - PrevtickStart) / (float)(Freq / SERVER_TICK_SPEED);
	m_Info.m_TickTime = (m_Info.m_CurrentTime - PrevtickStart) / (float)Freq;
	if(m_UpdateIntraTimesFunc)
		m_UpdateIntraTimesFunc();

	bool GotSnapshot = false;
	while(true)
	{
		int ChunkType, ChunkSize;
		if(ReadChunkHeader(&ChunkType, &ChunkSize, &ChunkTick))
		{
			// stop on error or eof
			if(m_pConsole)
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", "end of file");
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
				Stop();
#endif
			if(m_Info.m_PreviousTick == -1)
			{
				if(m_pConsole)
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", "empty demo");
				Stop();
			}
			else
				Pause();
			break;
		}

		// read the chunk
		int DataSize = 0;
		static char s_aData[CSnapshot::MAX_SIZE];
		if(ChunkSize)
		{
			static char s_aCompresseddata[CSnapshot::MAX_SIZE];
			if(io_read(m_File, s_aCompresseddata, ChunkSize) != (unsigned)ChunkSize)
			{
				// stop on error or eof
				if(m_pConsole)
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", "error reading chunk");
				Stop();
				break;
			}

			static char s_aDecompressed[CSnapshot::MAX_SIZE];
			DataSize = CNetBase::Decompress(s_aCompresseddata, ChunkSize, s_aDecompressed, sizeof(s_aDecompressed));
			if(DataSize < 0)
			{
				// stop on error or eof
				if(m_pConsole)
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", "error during network decompression");
				Stop();
				break;
			}

			DataSize = CVariableInt::Decompress(s_aDecompressed, DataSize, s_aData, sizeof(s_aData));

			if(DataSize < 0)
			{
				if(m_pConsole)
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", "error during intpack decompression");
				Stop();
				break;
			}
		}

		if(ChunkType == CHUNKTYPE_DELTA)
		{
			// process delta snapshot
			static char s_aNewsnap[CSnapshot::MAX_SIZE];
			CSnapshot *pNewsnap = (CSnapshot *)s_aNewsnap;
			DataSize = m_pSnapshotDelta->UnpackDelta((CSnapshot *)m_aLastSnapshotData, pNewsnap, s_aData, DataSize);

			if(DataSize < 0)
			{
				if(m_pConsole)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "error during unpacking of delta, err=%d", DataSize);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
				}
			}
			else if(!pNewsnap->IsValid(DataSize))
			{
				if(m_pConsole)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "snapshot delta invalid. DataSize=%d", DataSize);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
				}
			}
			else
			{
				if(m_pListener)
					m_pListener->OnDemoPlayerSnapshot(s_aNewsnap, DataSize);

				m_LastSnapshotDataSize = DataSize;
				mem_copy(m_aLastSnapshotData, s_aNewsnap, DataSize);
				GotSnapshot = true;
			}
		}
		else if(ChunkType == CHUNKTYPE_SNAPSHOT)
		{
			// process full snapshot
			CSnapshot *pSnap = (CSnapshot *)s_aData;
			if(!pSnap->IsValid(DataSize))
			{
				if(m_pConsole)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "snapshot invalid. DataSize=%d", DataSize);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
				}
			}
			else
			{
				GotSnapshot = true;

				m_LastSnapshotDataSize = DataSize;
				mem_copy(m_aLastSnapshotData, s_aData, DataSize);
				if(m_pListener)
					m_pListener->OnDemoPlayerSnapshot(s_aData, DataSize);
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
					m_pListener->OnDemoPlayerMessage(s_aData, DataSize);
			}
		}
	}
}

void CDemoPlayer::Pause()
{
	m_Info.m_Info.m_Paused = true;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && g_Config.m_ClVideoPauseWithDemo)
		IVideo::Current()->Pause(true);
#endif
}

void CDemoPlayer::Unpause()
{
	m_Info.m_Info.m_Paused = false;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && g_Config.m_ClVideoPauseWithDemo)
		IVideo::Current()->Pause(false);
#endif
}

int CDemoPlayer::Load(class IStorage *pStorage, class IConsole *pConsole, const char *pFilename, int StorageType)
{
	m_pConsole = pConsole;
	m_File = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType);
	if(!m_File)
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "could not open '%s'", pFilename);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", aBuf);
		}
		return -1;
	}

	// store the filename
	str_copy(m_aFilename, pFilename);

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

	// read the header
	io_read(m_File, &m_Info.m_Header, sizeof(m_Info.m_Header));
	if(mem_comp(m_Info.m_Header.m_aMarker, gs_aHeaderMarker, sizeof(gs_aHeaderMarker)) != 0)
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "'%s' is not a demo file", pFilename);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", aBuf);
		}
		io_close(m_File);
		m_File = 0;
		return -1;
	}

	if(m_Info.m_Header.m_Version < gs_OldVersion)
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "demo version %d is not supported", m_Info.m_Header.m_Version);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", aBuf);
		}
		io_close(m_File);
		m_File = 0;
		return -1;
	}
	else if(m_Info.m_Header.m_Version > gs_OldVersion)
		io_read(m_File, &m_Info.m_TimelineMarkers, sizeof(m_Info.m_TimelineMarkers));

	SHA256_DIGEST Sha256 = SHA256_ZEROED;
	if(m_Info.m_Header.m_Version >= gs_Sha256Version)
	{
		CUuid ExtensionUuid = {};
		io_read(m_File, &ExtensionUuid.m_aData, sizeof(ExtensionUuid.m_aData));

		if(ExtensionUuid == SHA256_EXTENSION)
		{
			io_read(m_File, &Sha256, sizeof(SHA256_DIGEST)); // need a safe read
		}
		else
		{
			// This hopes whatever happened during the version increment didn't add something here
			dbg_msg("demo", "demo version incremented, but not by ddnet");
			io_seek(m_File, -(int)sizeof(ExtensionUuid.m_aData), IOSEEK_CUR);
		}
	}

	// get demo type
	if(!str_comp(m_Info.m_Header.m_aType, "client"))
		m_DemoType = DEMOTYPE_CLIENT;
	else if(!str_comp(m_Info.m_Header.m_aType, "server"))
		m_DemoType = DEMOTYPE_SERVER;
	else
		m_DemoType = DEMOTYPE_INVALID;

	// read map
	unsigned MapSize = bytes_be_to_uint(m_Info.m_Header.m_aMapSize);

	// check if we already have the map
	// TODO: improve map checking (maps folder, check crc)
	unsigned Crc = bytes_be_to_uint(m_Info.m_Header.m_aMapCrc);

	// save byte offset of map for later use
	m_MapOffset = io_tell(m_File);
	io_skip(m_File, MapSize);

	// store map information
	m_MapInfo.m_Crc = Crc;
	m_MapInfo.m_Sha256 = Sha256;
	m_MapInfo.m_Size = MapSize;
	str_copy(m_MapInfo.m_aName, m_Info.m_Header.m_aMapName);

	if(m_Info.m_Header.m_Version > gs_OldVersion)
	{
		// get timeline markers
		int Num = bytes_be_to_int(m_Info.m_TimelineMarkers.m_aNumTimelineMarkers);
		m_Info.m_Info.m_NumTimelineMarkers = clamp<int>(Num, 0, MAX_TIMELINE_MARKERS);
		for(int i = 0; i < m_Info.m_Info.m_NumTimelineMarkers; i++)
		{
			m_Info.m_Info.m_aTimelineMarkers[i] = bytes_be_to_int(m_Info.m_TimelineMarkers.m_aTimelineMarkers[i]);
		}
	}

	// scan the file for interesting points
	ScanFile();

	// reset slice markers
	g_Config.m_ClDemoSliceBegin = -1;
	g_Config.m_ClDemoSliceEnd = -1;

	// ready for playback
	return 0;
}

unsigned char *CDemoPlayer::GetMapData(class IStorage *pStorage)
{
	if(!m_MapInfo.m_Size)
		return 0;

	long CurSeek = io_tell(m_File);

	// get map data
	io_seek(m_File, m_MapOffset, IOSEEK_START);
	unsigned char *pMapData = (unsigned char *)malloc(m_MapInfo.m_Size);
	io_read(m_File, pMapData, m_MapInfo.m_Size);
	io_seek(m_File, CurSeek, IOSEEK_START);
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
	char aSha[SHA256_MAXSTRSIZE], aMapFilename[128];
	sha256_str(Sha256, aSha, sizeof(aSha));
	str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s_%s.map", m_Info.m_Header.m_aMapName, aSha);

	// save map
	IOHANDLE MapFile = pStorage->OpenFile(aMapFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!MapFile)
		return false;

	io_write(MapFile, pMapData, m_MapInfo.m_Size);
	io_close(MapFile);

	// free data
	free(pMapData);
	return true;
}

int64_t CDemoPlayer::Time()
{
#if defined(CONF_VIDEORECORDER)
	static bool s_Recording = false;
	if(IVideo::Current())
	{
		if(!s_Recording)
		{
			s_Recording = true;
			m_Info.m_LastUpdate = IVideo::Time();
		}
		return IVideo::Time();
	}
	else
	{
		int64_t Now = time_get();
		if(s_Recording)
		{
			s_Recording = false;
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
	int WantedTick = m_Info.m_Info.m_FirstTick + ((m_Info.m_Info.m_LastTick - m_Info.m_Info.m_FirstTick) * Percent);
	return SetPos(WantedTick);
}

int CDemoPlayer::SeekTime(float Seconds)
{
	int WantedTick = m_Info.m_Info.m_CurrentTick + (Seconds * (float)SERVER_TICK_SPEED);
	return SetPos(WantedTick);
}

int CDemoPlayer::SeekTick(ETickOffset TickOffset)
{
	return SetPos(m_Info.m_Info.m_CurrentTick + (int)TickOffset);
}

int CDemoPlayer::SetPos(int WantedTick)
{
	if(!m_File)
		return -1;

	WantedTick = clamp(WantedTick, m_Info.m_Info.m_FirstTick, m_Info.m_Info.m_LastTick);
	const int KeyFrameWantedTick = WantedTick - 5; // -5 because we have to have a current tick and previous tick when we do the playback
	const float Percent = (KeyFrameWantedTick - m_Info.m_Info.m_FirstTick) / (float)(m_Info.m_Info.m_LastTick - m_Info.m_Info.m_FirstTick);

	// get correct key frame
	int KeyFrame = clamp((int)(m_Info.m_SeekablePoints * Percent), 0, m_Info.m_SeekablePoints - 1);
	while(KeyFrame < m_Info.m_SeekablePoints - 1 && m_pKeyFrames[KeyFrame].m_Tick < KeyFrameWantedTick)
		KeyFrame++;
	while(KeyFrame > 0 && m_pKeyFrames[KeyFrame].m_Tick > KeyFrameWantedTick)
		KeyFrame--;

	// seek to the correct key frame
	io_seek(m_File, m_pKeyFrames[KeyFrame].m_Filepos, IOSEEK_START);

	m_Info.m_NextTick = -1;
	m_Info.m_Info.m_CurrentTick = -1;
	m_Info.m_PreviousTick = -1;

	// playback everything until we hit our tick
	while(m_Info.m_NextTick < WantedTick)
		DoTick();

	Play();

	return 0;
}

void CDemoPlayer::SetSpeed(float Speed)
{
	m_Info.m_Info.m_Speed = clamp(Speed, 0.f, 256.f);
}

void CDemoPlayer::SetSpeedIndex(int Offset)
{
	m_SpeedIndex = clamp(m_SpeedIndex + Offset, 0, (int)(std::size(g_aSpeeds) - 1));
	SetSpeed(g_aSpeeds[m_SpeedIndex]);
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

		while(!m_Info.m_Info.m_Paused)
		{
			int64_t CurtickStart = (m_Info.m_Info.m_CurrentTick) * Freq / SERVER_TICK_SPEED;

			// break if we are ready
			if(RealTime && CurtickStart > m_Info.m_CurrentTime)
				break;

			// do one more tick
			DoTick();
		}
	}

	// update intratick
	{
		int64_t CurtickStart = (m_Info.m_Info.m_CurrentTick) * Freq / SERVER_TICK_SPEED;
		int64_t PrevtickStart = (m_Info.m_PreviousTick) * Freq / SERVER_TICK_SPEED;
		m_Info.m_IntraTick = (m_Info.m_CurrentTime - PrevtickStart) / (float)(CurtickStart - PrevtickStart);
		m_Info.m_IntraTickSincePrev = (m_Info.m_CurrentTime - PrevtickStart) / (float)(Freq / SERVER_TICK_SPEED);
		m_Info.m_TickTime = (m_Info.m_CurrentTime - PrevtickStart) / (float)Freq;
		if(m_UpdateIntraTimesFunc)
			m_UpdateIntraTimesFunc();
	}

	if(m_Info.m_Info.m_CurrentTick == m_Info.m_PreviousTick || m_Info.m_Info.m_CurrentTick == m_Info.m_NextTick)
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "tick error prev=%d cur=%d next=%d",
				m_Info.m_PreviousTick, m_Info.m_Info.m_CurrentTick, m_Info.m_NextTick);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "demo_player", aBuf);
		}
	}

	return 0;
}

int CDemoPlayer::Stop()
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		IVideo::Current()->Stop();
#endif

	if(!m_File)
		return -1;

	if(m_pConsole)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_player", "Stopped playback");
	io_close(m_File);
	m_File = 0;
	free(m_pKeyFrames);
	m_pKeyFrames = 0;
	str_copy(m_aFilename, "");
	return 0;
}

void CDemoPlayer::GetDemoName(char *pBuffer, int BufferSize) const
{
	const char *pFileName = m_aFilename;
	const char *pExtractedName = pFileName;
	const char *pEnd = 0;
	for(; *pFileName; ++pFileName)
	{
		if(*pFileName == '/' || *pFileName == '\\')
			pExtractedName = pFileName + 1;
		else if(*pFileName == '.')
			pEnd = pFileName;
	}

	int Length = pEnd > pExtractedName ? minimum(BufferSize, (int)(pEnd - pExtractedName + 1)) : BufferSize;
	str_copy(pBuffer, pExtractedName, Length);
}

bool CDemoPlayer::GetDemoInfo(class IStorage *pStorage, const char *pFilename, int StorageType, CDemoHeader *pDemoHeader, CTimelineMarkers *pTimelineMarkers, CMapInfo *pMapInfo) const
{
	if(!pDemoHeader || !pTimelineMarkers || !pMapInfo)
		return false;

	mem_zero(pDemoHeader, sizeof(CDemoHeader));
	mem_zero(pTimelineMarkers, sizeof(CTimelineMarkers));

	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType);
	if(!File)
		return false;

	io_read(File, pDemoHeader, sizeof(CDemoHeader));
	io_read(File, pTimelineMarkers, sizeof(CTimelineMarkers));

	str_copy(pMapInfo->m_aName, pDemoHeader->m_aMapName);
	pMapInfo->m_Crc = bytes_be_to_int(pDemoHeader->m_aMapCrc);

	SHA256_DIGEST Sha256 = SHA256_ZEROED;
	if(pDemoHeader->m_Version >= gs_Sha256Version)
	{
		CUuid ExtensionUuid = {};
		io_read(File, &ExtensionUuid.m_aData, sizeof(ExtensionUuid.m_aData));

		if(ExtensionUuid == SHA256_EXTENSION)
		{
			io_read(File, &Sha256, sizeof(SHA256_DIGEST)); // need a safe read
		}
		else
		{
			// This hopes whatever happened during the version increment didn't add something here
			dbg_msg("demo", "demo version incremented, but not by ddnet");
			io_seek(File, -(int)sizeof(ExtensionUuid.m_aData), IOSEEK_CUR);
		}
	}
	pMapInfo->m_Sha256 = Sha256;

	pMapInfo->m_Size = bytes_be_to_int(pDemoHeader->m_aMapSize);

	io_close(File);
	return !(mem_comp(pDemoHeader->m_aMarker, gs_aHeaderMarker, sizeof(gs_aHeaderMarker)) || pDemoHeader->m_Version < gs_OldVersion);
}

int CDemoPlayer::GetDemoType() const
{
	if(m_File)
		return m_DemoType;
	return DEMOTYPE_INVALID;
}

void CDemoEditor::Init(const char *pNetVersion, class CSnapshotDelta *pSnapshotDelta, class IConsole *pConsole, class IStorage *pStorage)
{
	m_pNetVersion = pNetVersion;
	m_pSnapshotDelta = pSnapshotDelta;
	m_pConsole = pConsole;
	m_pStorage = pStorage;
}

void CDemoEditor::Slice(const char *pDemo, const char *pDst, int StartTick, int EndTick, DEMOFUNC_FILTER pfnFilter, void *pUser)
{
	class CDemoPlayer DemoPlayer(m_pSnapshotDelta);
	class CDemoRecorder DemoRecorder(m_pSnapshotDelta);

	m_pDemoPlayer = &DemoPlayer;
	m_pDemoRecorder = &DemoRecorder;

	m_pDemoPlayer->SetListener(this);

	m_SliceFrom = StartTick;
	m_SliceTo = EndTick;
	m_Stop = false;

	if(m_pDemoPlayer->Load(m_pStorage, m_pConsole, pDemo, IStorage::TYPE_ALL) == -1)
		return;

	const CMapInfo *pMapInfo = m_pDemoPlayer->GetMapInfo();
	const CDemoPlayer::CPlaybackInfo *pInfo = m_pDemoPlayer->Info();

	SHA256_DIGEST Sha256 = pMapInfo->m_Sha256;
	if(pInfo->m_Header.m_Version < gs_Sha256Version)
	{
		if(m_pDemoPlayer->ExtractMap(m_pStorage))
			Sha256 = pMapInfo->m_Sha256;
	}

	unsigned char *pMapData = m_pDemoPlayer->GetMapData(m_pStorage);
	const int Result = m_pDemoRecorder->Start(m_pStorage, m_pConsole, pDst, m_pNetVersion, pMapInfo->m_aName, &Sha256, pMapInfo->m_Crc, "client", pMapInfo->m_Size, pMapData, NULL, pfnFilter, pUser) == -1;
	free(pMapData);
	if(Result != 0)
		return;

	m_pDemoPlayer->Play();

	while(m_pDemoPlayer->IsPlaying() && !m_Stop)
	{
		m_pDemoPlayer->Update(false);

		if(pInfo->m_Info.m_Paused)
			break;
	}

	m_pDemoPlayer->Stop();
	m_pDemoRecorder->Stop();
} // NOLINT(clang-analyzer-unix.Malloc)

void CDemoEditor::OnDemoPlayerSnapshot(void *pData, int Size)
{
	const CDemoPlayer::CPlaybackInfo *pInfo = m_pDemoPlayer->Info();

	if(m_SliceTo != -1 && pInfo->m_Info.m_CurrentTick > m_SliceTo)
		m_Stop = true;
	else if(m_SliceFrom == -1 || pInfo->m_Info.m_CurrentTick >= m_SliceFrom)
		m_pDemoRecorder->RecordSnapshot(pInfo->m_Info.m_CurrentTick, pData, Size);
}

void CDemoEditor::OnDemoPlayerMessage(void *pData, int Size)
{
	const CDemoPlayer::CPlaybackInfo *pInfo = m_pDemoPlayer->Info();

	if(m_SliceTo != -1 && pInfo->m_Info.m_CurrentTick > m_SliceTo)
		m_Stop = true;
	else if(m_SliceFrom == -1 || pInfo->m_Info.m_CurrentTick >= m_SliceFrom)
		m_pDemoRecorder->RecordMessage(pData, Size);
}
