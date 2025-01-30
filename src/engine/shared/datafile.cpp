/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "datafile.h"

#include <base/hash_ctxt.h>
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/storage.h>

#include "uuid_manager.h"

#include <cstdlib>
#include <limits>

#include <zlib.h>

static const int DEBUG = 0;

enum
{
	OFFSET_UUID_TYPE = 0x8000,
};

struct CItemEx
{
	int m_aUuid[sizeof(CUuid) / sizeof(int32_t)];

	static CItemEx FromUuid(CUuid Uuid)
	{
		CItemEx Result;
		for(size_t i = 0; i < sizeof(CUuid) / sizeof(int32_t); i++)
			Result.m_aUuid[i] = bytes_be_to_uint(&Uuid.m_aData[i * sizeof(int32_t)]);
		return Result;
	}

	CUuid ToUuid() const
	{
		CUuid Result;
		for(size_t i = 0; i < sizeof(CUuid) / sizeof(int32_t); i++)
			uint_to_bytes_be(&Result.m_aData[i * sizeof(int32_t)], m_aUuid[i]);
		return Result;
	}
};

struct CDatafileItemType
{
	int m_Type;
	int m_Start;
	int m_Num;
};

struct CDatafileItem
{
	int m_TypeAndId;
	int m_Size;
};

struct CDatafileHeader
{
	char m_aId[4];
	int m_Version;
	int m_Size;
	int m_Swaplen;
	int m_NumItemTypes;
	int m_NumItems;
	int m_NumRawData;
	int m_ItemSize;
	int m_DataSize;

	constexpr size_t SizeOffset()
	{
		// The size of these members is not included in m_Size and m_Swaplen
		return sizeof(m_aId) + sizeof(m_Version) + sizeof(m_Size) + sizeof(m_Swaplen);
	}
};

struct CDatafileInfo
{
	CDatafileItemType *m_pItemTypes;
	int *m_pItemOffsets;
	int *m_pDataOffsets;
	int *m_pDataSizes;

	char *m_pItemStart;
	char *m_pDataStart;
};

struct CDatafile
{
	IOHANDLE m_File;
	SHA256_DIGEST m_Sha256;
	unsigned m_Crc;
	CDatafileInfo m_Info;
	CDatafileHeader m_Header;
	int m_DataStartOffset;
	char **m_ppDataPtrs;
	int *m_pDataSizes;
	char *m_pData;
};

bool CDataFileReader::Open(class IStorage *pStorage, const char *pFilename, int StorageType)
{
	dbg_assert(m_pDataFile == nullptr, "File already open");

	log_trace("datafile", "loading. filename='%s'", pFilename);

	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType);
	if(!File)
	{
		dbg_msg("datafile", "could not open '%s'", pFilename);
		return false;
	}

	// take the CRC of the file and store it
	unsigned Crc = 0;
	SHA256_DIGEST Sha256;
	{
		enum
		{
			BUFFER_SIZE = 64 * 1024
		};

		SHA256_CTX Sha256Ctxt;
		sha256_init(&Sha256Ctxt);
		unsigned char aBuffer[BUFFER_SIZE];

		while(true)
		{
			unsigned Bytes = io_read(File, aBuffer, BUFFER_SIZE);
			if(Bytes == 0)
				break;
			Crc = crc32(Crc, aBuffer, Bytes);
			sha256_update(&Sha256Ctxt, aBuffer, Bytes);
		}
		Sha256 = sha256_finish(&Sha256Ctxt);

		io_seek(File, 0, IOSEEK_START);
	}

	// TODO: change this header
	CDatafileHeader Header;
	if(sizeof(Header) != io_read(File, &Header, sizeof(Header)))
	{
		dbg_msg("datafile", "couldn't load header");
		return false;
	}
	if(Header.m_aId[0] != 'A' || Header.m_aId[1] != 'T' || Header.m_aId[2] != 'A' || Header.m_aId[3] != 'D')
	{
		if(Header.m_aId[0] != 'D' || Header.m_aId[1] != 'A' || Header.m_aId[2] != 'T' || Header.m_aId[3] != 'A')
		{
			dbg_msg("datafile", "wrong signature. %x %x %x %x", Header.m_aId[0], Header.m_aId[1], Header.m_aId[2], Header.m_aId[3]);
			return false;
		}
	}

#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(&Header, sizeof(int), sizeof(Header) / sizeof(int));
#endif
	if(Header.m_Version != 3 && Header.m_Version != 4)
	{
		dbg_msg("datafile", "wrong version. version=%x", Header.m_Version);
		return false;
	}

	// read in the rest except the data
	unsigned Size = 0;
	Size += Header.m_NumItemTypes * sizeof(CDatafileItemType);
	Size += (Header.m_NumItems + Header.m_NumRawData) * sizeof(int);
	if(Header.m_Version == 4)
		Size += Header.m_NumRawData * sizeof(int); // v4 has uncompressed data sizes as well
	Size += Header.m_ItemSize;

	unsigned AllocSize = Size;
	AllocSize += sizeof(CDatafile); // add space for info structure
	AllocSize += Header.m_NumRawData * sizeof(void *); // add space for data pointers
	AllocSize += Header.m_NumRawData * sizeof(int); // add space for data sizes
	if(Size > (((int64_t)1) << 31) || Header.m_NumItemTypes < 0 || Header.m_NumItems < 0 || Header.m_NumRawData < 0 || Header.m_ItemSize < 0)
	{
		io_close(File);
		dbg_msg("datafile", "unable to load file, invalid file information");
		return false;
	}

	CDatafile *pTmpDataFile = (CDatafile *)malloc(AllocSize);
	pTmpDataFile->m_Header = Header;
	pTmpDataFile->m_DataStartOffset = sizeof(CDatafileHeader) + Size;
	pTmpDataFile->m_ppDataPtrs = (char **)(pTmpDataFile + 1);
	pTmpDataFile->m_pDataSizes = (int *)(pTmpDataFile->m_ppDataPtrs + Header.m_NumRawData);
	pTmpDataFile->m_pData = (char *)(pTmpDataFile->m_pDataSizes + Header.m_NumRawData);
	pTmpDataFile->m_File = File;
	pTmpDataFile->m_Sha256 = Sha256;
	pTmpDataFile->m_Crc = Crc;

	// clear the data pointers and sizes
	mem_zero(pTmpDataFile->m_ppDataPtrs, Header.m_NumRawData * sizeof(void *));
	mem_zero(pTmpDataFile->m_pDataSizes, Header.m_NumRawData * sizeof(int));

	// read types, offsets, sizes and item data
	unsigned ReadSize = io_read(File, pTmpDataFile->m_pData, Size);
	if(ReadSize != Size)
	{
		io_close(pTmpDataFile->m_File);
		free(pTmpDataFile);
		dbg_msg("datafile", "couldn't load the whole thing, wanted=%d got=%d", Size, ReadSize);
		return false;
	}

	m_pDataFile = pTmpDataFile;

#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(m_pDataFile->m_pData, sizeof(int), minimum(static_cast<unsigned>(Header.m_Swaplen), Size) / sizeof(int));
#endif

	if(DEBUG)
	{
		dbg_msg("datafile", "allocsize=%d", AllocSize);
		dbg_msg("datafile", "readsize=%d", ReadSize);
		dbg_msg("datafile", "swaplen=%d", Header.m_Swaplen);
		dbg_msg("datafile", "item_size=%d", m_pDataFile->m_Header.m_ItemSize);
	}

	m_pDataFile->m_Info.m_pItemTypes = (CDatafileItemType *)m_pDataFile->m_pData;
	m_pDataFile->m_Info.m_pItemOffsets = (int *)&m_pDataFile->m_Info.m_pItemTypes[m_pDataFile->m_Header.m_NumItemTypes];
	m_pDataFile->m_Info.m_pDataOffsets = &m_pDataFile->m_Info.m_pItemOffsets[m_pDataFile->m_Header.m_NumItems];
	m_pDataFile->m_Info.m_pDataSizes = &m_pDataFile->m_Info.m_pDataOffsets[m_pDataFile->m_Header.m_NumRawData];

	if(Header.m_Version == 4)
		m_pDataFile->m_Info.m_pItemStart = (char *)&m_pDataFile->m_Info.m_pDataSizes[m_pDataFile->m_Header.m_NumRawData];
	else
		m_pDataFile->m_Info.m_pItemStart = (char *)&m_pDataFile->m_Info.m_pDataOffsets[m_pDataFile->m_Header.m_NumRawData];
	m_pDataFile->m_Info.m_pDataStart = m_pDataFile->m_Info.m_pItemStart + m_pDataFile->m_Header.m_ItemSize;

	log_trace("datafile", "loading done. datafile='%s'", pFilename);

	return true;
}

bool CDataFileReader::Close()
{
	if(!m_pDataFile)
		return true;

	// free the data that is loaded
	for(int i = 0; i < m_pDataFile->m_Header.m_NumRawData; i++)
	{
		free(m_pDataFile->m_ppDataPtrs[i]);
		m_pDataFile->m_ppDataPtrs[i] = nullptr;
		m_pDataFile->m_pDataSizes[i] = 0;
	}

	io_close(m_pDataFile->m_File);
	free(m_pDataFile);
	m_pDataFile = nullptr;
	return true;
}

IOHANDLE CDataFileReader::File() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_File;
}

int CDataFileReader::NumData() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_Header.m_NumRawData;
}

// returns the size in the file
int CDataFileReader::GetFileDataSize(int Index) const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index == m_pDataFile->m_Header.m_NumRawData - 1)
		return m_pDataFile->m_Header.m_DataSize - m_pDataFile->m_Info.m_pDataOffsets[Index];

	return m_pDataFile->m_Info.m_pDataOffsets[Index + 1] - m_pDataFile->m_Info.m_pDataOffsets[Index];
}

// returns the size of the resulting data
int CDataFileReader::GetDataSize(int Index) const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index < 0 || Index >= m_pDataFile->m_Header.m_NumRawData)
	{
		return 0;
	}

	if(!m_pDataFile->m_ppDataPtrs[Index])
	{
		if(m_pDataFile->m_Header.m_Version >= 4)
		{
			return m_pDataFile->m_Info.m_pDataSizes[Index];
		}
		else
		{
			return GetFileDataSize(Index);
		}
	}
	const int Size = m_pDataFile->m_pDataSizes[Index];
	if(Size < 0)
		return 0; // summarize all errors as zero size
	return Size;
}

void *CDataFileReader::GetDataImpl(int Index, bool Swap)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index < 0 || Index >= m_pDataFile->m_Header.m_NumRawData)
		return nullptr;

	// load it if needed
	if(!m_pDataFile->m_ppDataPtrs[Index])
	{
		// don't try to load again if it previously failed
		if(m_pDataFile->m_pDataSizes[Index] < 0)
			return nullptr;

		// fetch the data size
		unsigned DataSize = GetFileDataSize(Index);
#if defined(CONF_ARCH_ENDIAN_BIG)
		unsigned SwapSize = DataSize;
#endif

		if(m_pDataFile->m_Header.m_Version == 4)
		{
			// v4 has compressed data
			const unsigned OriginalUncompressedSize = m_pDataFile->m_Info.m_pDataSizes[Index];
			unsigned long UncompressedSize = OriginalUncompressedSize;

			log_trace("datafile", "loading data. index=%d size=%u uncompressed=%u", Index, DataSize, OriginalUncompressedSize);

			// read the compressed data
			void *pCompressedData = malloc(DataSize);
			unsigned ActualDataSize = 0;
			if(io_seek(m_pDataFile->m_File, m_pDataFile->m_DataStartOffset + m_pDataFile->m_Info.m_pDataOffsets[Index], IOSEEK_START) == 0)
				ActualDataSize = io_read(m_pDataFile->m_File, pCompressedData, DataSize);
			if(DataSize != ActualDataSize)
			{
				log_error("datafile", "truncation error, could not read all data. index=%d wanted=%u got=%u", Index, DataSize, ActualDataSize);
				free(pCompressedData);
				m_pDataFile->m_ppDataPtrs[Index] = nullptr;
				m_pDataFile->m_pDataSizes[Index] = -1;
				return nullptr;
			}

			// decompress the data
			m_pDataFile->m_ppDataPtrs[Index] = (char *)malloc(UncompressedSize);
			m_pDataFile->m_pDataSizes[Index] = UncompressedSize;
			const int Result = uncompress((Bytef *)m_pDataFile->m_ppDataPtrs[Index], &UncompressedSize, (Bytef *)pCompressedData, DataSize);
			free(pCompressedData);
			if(Result != Z_OK || UncompressedSize != OriginalUncompressedSize)
			{
				log_error("datafile", "uncompress error. result=%d wanted=%u got=%lu", Result, OriginalUncompressedSize, UncompressedSize);
				free(m_pDataFile->m_ppDataPtrs[Index]);
				m_pDataFile->m_ppDataPtrs[Index] = nullptr;
				m_pDataFile->m_pDataSizes[Index] = -1;
				return nullptr;
			}

#if defined(CONF_ARCH_ENDIAN_BIG)
			SwapSize = UncompressedSize;
#endif
		}
		else
		{
			// load the data
			log_trace("datafile", "loading data. index=%d size=%d", Index, DataSize);
			m_pDataFile->m_ppDataPtrs[Index] = static_cast<char *>(malloc(DataSize));
			m_pDataFile->m_pDataSizes[Index] = DataSize;
			unsigned ActualDataSize = 0;
			if(io_seek(m_pDataFile->m_File, m_pDataFile->m_DataStartOffset + m_pDataFile->m_Info.m_pDataOffsets[Index], IOSEEK_START) == 0)
				ActualDataSize = io_read(m_pDataFile->m_File, m_pDataFile->m_ppDataPtrs[Index], DataSize);
			if(DataSize != ActualDataSize)
			{
				log_error("datafile", "truncation error, could not read all data. index=%d wanted=%u got=%u", Index, DataSize, ActualDataSize);
				free(m_pDataFile->m_ppDataPtrs[Index]);
				m_pDataFile->m_ppDataPtrs[Index] = nullptr;
				m_pDataFile->m_pDataSizes[Index] = -1;
				return nullptr;
			}
		}

#if defined(CONF_ARCH_ENDIAN_BIG)
		if(Swap && SwapSize)
			swap_endian(m_pDataFile->m_ppDataPtrs[Index], sizeof(int), SwapSize / sizeof(int));
#endif
	}

	return m_pDataFile->m_ppDataPtrs[Index];
}

void *CDataFileReader::GetData(int Index)
{
	return GetDataImpl(Index, false);
}

void *CDataFileReader::GetDataSwapped(int Index)
{
	return GetDataImpl(Index, true);
}

const char *CDataFileReader::GetDataString(int Index)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index == -1)
		return "";
	const int DataSize = GetDataSize(Index);
	if(!DataSize)
		return nullptr;
	const char *pData = static_cast<char *>(GetData(Index));
	if(pData == nullptr || mem_has_null(pData, DataSize - 1) || pData[DataSize - 1] != '\0' || !str_utf8_check(pData))
		return nullptr;
	return pData;
}

void CDataFileReader::ReplaceData(int Index, char *pData, size_t Size)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");
	dbg_assert(Index >= 0 && Index < m_pDataFile->m_Header.m_NumRawData, "Index invalid");

	free(m_pDataFile->m_ppDataPtrs[Index]);
	m_pDataFile->m_ppDataPtrs[Index] = pData;
	m_pDataFile->m_pDataSizes[Index] = Size;
}

void CDataFileReader::UnloadData(int Index)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index < 0 || Index >= m_pDataFile->m_Header.m_NumRawData)
		return;

	free(m_pDataFile->m_ppDataPtrs[Index]);
	m_pDataFile->m_ppDataPtrs[Index] = nullptr;
	m_pDataFile->m_pDataSizes[Index] = 0;
}

int CDataFileReader::GetItemSize(int Index) const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index == m_pDataFile->m_Header.m_NumItems - 1)
		return m_pDataFile->m_Header.m_ItemSize - m_pDataFile->m_Info.m_pItemOffsets[Index] - sizeof(CDatafileItem);
	return m_pDataFile->m_Info.m_pItemOffsets[Index + 1] - m_pDataFile->m_Info.m_pItemOffsets[Index] - sizeof(CDatafileItem);
}

int CDataFileReader::GetExternalItemType(int InternalType, CUuid *pUuid)
{
	if(InternalType <= OFFSET_UUID_TYPE || InternalType == ITEMTYPE_EX)
	{
		if(pUuid)
			*pUuid = UUID_ZEROED;
		return InternalType;
	}
	int TypeIndex = FindItemIndex(ITEMTYPE_EX, InternalType);
	if(TypeIndex < 0 || GetItemSize(TypeIndex) < (int)sizeof(CItemEx))
	{
		if(pUuid)
			*pUuid = UUID_ZEROED;
		return InternalType;
	}
	const CItemEx *pItemEx = (const CItemEx *)GetItem(TypeIndex);
	CUuid Uuid = pItemEx->ToUuid();
	if(pUuid)
		*pUuid = Uuid;
	// Propagate UUID_UNKNOWN, it doesn't hurt.
	return g_UuidManager.LookupUuid(Uuid);
}

int CDataFileReader::GetInternalItemType(int ExternalType)
{
	if(ExternalType < OFFSET_UUID)
	{
		return ExternalType;
	}
	CUuid Uuid = g_UuidManager.GetUuid(ExternalType);
	int Start, Num;
	GetType(ITEMTYPE_EX, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		if(GetItemSize(i) < (int)sizeof(CItemEx))
		{
			continue;
		}
		int Id;
		if(Uuid == ((const CItemEx *)GetItem(i, nullptr, &Id))->ToUuid())
		{
			return Id;
		}
	}
	return -1;
}

void *CDataFileReader::GetItem(int Index, int *pType, int *pId, CUuid *pUuid)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	CDatafileItem *pItem = (CDatafileItem *)(m_pDataFile->m_Info.m_pItemStart + m_pDataFile->m_Info.m_pItemOffsets[Index]);

	// remove sign extension
	const int Type = GetExternalItemType((pItem->m_TypeAndId >> 16) & 0xffff, pUuid);
	if(pType)
	{
		*pType = Type;
	}
	if(pId)
	{
		*pId = pItem->m_TypeAndId & 0xffff;
	}
	return (void *)(pItem + 1);
}

void CDataFileReader::GetType(int Type, int *pStart, int *pNum)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	*pStart = 0;
	*pNum = 0;

	Type = GetInternalItemType(Type);
	for(int i = 0; i < m_pDataFile->m_Header.m_NumItemTypes; i++)
	{
		if(m_pDataFile->m_Info.m_pItemTypes[i].m_Type == Type)
		{
			*pStart = m_pDataFile->m_Info.m_pItemTypes[i].m_Start;
			*pNum = m_pDataFile->m_Info.m_pItemTypes[i].m_Num;
			return;
		}
	}
}

int CDataFileReader::FindItemIndex(int Type, int Id)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	int Start, Num;
	GetType(Type, &Start, &Num);
	for(int i = 0; i < Num; i++)
	{
		int ItemId;
		GetItem(Start + i, nullptr, &ItemId);
		if(Id == ItemId)
		{
			return Start + i;
		}
	}
	return -1;
}

void *CDataFileReader::FindItem(int Type, int Id)
{
	int Index = FindItemIndex(Type, Id);
	if(Index < 0)
	{
		return nullptr;
	}
	return GetItem(Index);
}

int CDataFileReader::NumItems() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_Header.m_NumItems;
}

SHA256_DIGEST CDataFileReader::Sha256() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_Sha256;
}

unsigned CDataFileReader::Crc() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_Crc;
}

int CDataFileReader::MapSize() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_Header.m_Size + m_pDataFile->m_Header.SizeOffset();
}

CDataFileWriter::CDataFileWriter()
{
	m_File = nullptr;
}

CDataFileWriter::~CDataFileWriter()
{
	if(m_File)
	{
		io_close(m_File);
		m_File = nullptr;
	}

	for(CItemInfo &ItemInfo : m_vItems)
	{
		free(ItemInfo.m_pData);
	}

	for(CDataInfo &DataInfo : m_vDatas)
	{
		free(DataInfo.m_pUncompressedData);
		free(DataInfo.m_pCompressedData);
	}
}

bool CDataFileWriter::Open(class IStorage *pStorage, const char *pFilename, int StorageType)
{
	dbg_assert(!m_File, "File already open");
	m_File = pStorage->OpenFile(pFilename, IOFLAG_WRITE, StorageType);
	return m_File != nullptr;
}

int CDataFileWriter::GetTypeFromIndex(int Index) const
{
	return ITEMTYPE_EX - Index - 1;
}

int CDataFileWriter::GetExtendedItemTypeIndex(int Type, const CUuid *pUuid)
{
	int Index = 0;
	if(Type == -1)
	{
		// Unknown type, search for UUID
		for(const auto &ExtendedItemType : m_vExtendedItemTypes)
		{
			if(ExtendedItemType.m_Uuid == *pUuid)
				return Index;
			++Index;
		}
	}
	else
	{
		for(const auto &ExtendedItemType : m_vExtendedItemTypes)
		{
			if(ExtendedItemType.m_Type == Type)
				return Index;
			++Index;
		}
	}

	// Type not found, add it.
	CExtendedItemType ExtendedType;
	ExtendedType.m_Type = Type;
	ExtendedType.m_Uuid = Type == -1 ? *pUuid : g_UuidManager.GetUuid(Type);
	m_vExtendedItemTypes.push_back(ExtendedType);

	CItemEx ItemEx = CItemEx::FromUuid(ExtendedType.m_Uuid);
	AddItem(ITEMTYPE_EX, GetTypeFromIndex(Index), sizeof(ItemEx), &ItemEx);
	return Index;
}

int CDataFileWriter::AddItem(int Type, int Id, size_t Size, const void *pData, const CUuid *pUuid)
{
	dbg_assert((Type >= 0 && Type < MAX_ITEM_TYPES) || Type >= OFFSET_UUID || (Type == -1 && pUuid != nullptr), "Invalid type");
	dbg_assert(Id >= 0 && Id <= ITEMTYPE_EX, "Invalid ID");
	dbg_assert(Size == 0 || pData != nullptr, "Data missing"); // Items without data are allowed
	dbg_assert(Size <= (size_t)std::numeric_limits<int>::max(), "Data too large");
	dbg_assert(Size % sizeof(int) == 0, "Invalid data boundary");

	if(Type == -1 || Type >= OFFSET_UUID)
	{
		Type = GetTypeFromIndex(GetExtendedItemTypeIndex(Type, pUuid));
	}

	const int NumItems = m_vItems.size();
	m_vItems.emplace_back();
	CItemInfo &Info = m_vItems.back();
	Info.m_Type = Type;
	Info.m_Id = Id;
	Info.m_Size = Size;

	// copy data
	if(Size > 0)
	{
		Info.m_pData = malloc(Size);
		mem_copy(Info.m_pData, pData, Size);
	}
	else
		Info.m_pData = nullptr;

	// link
	CItemTypeInfo &ItemType = m_ItemTypes[Type];
	Info.m_Prev = ItemType.m_Last;
	Info.m_Next = -1;

	if(ItemType.m_Last != -1)
		m_vItems[ItemType.m_Last].m_Next = NumItems;
	ItemType.m_Last = NumItems;

	if(ItemType.m_First == -1)
		ItemType.m_First = NumItems;

	ItemType.m_Num++;
	return NumItems;
}

int CDataFileWriter::AddData(size_t Size, const void *pData, ECompressionLevel CompressionLevel)
{
	dbg_assert(Size > 0 && pData != nullptr, "Data missing");
	dbg_assert(Size <= (size_t)std::numeric_limits<int>::max(), "Data too large");

	m_vDatas.emplace_back();
	CDataInfo &Info = m_vDatas.back();
	Info.m_pUncompressedData = malloc(Size);
	mem_copy(Info.m_pUncompressedData, pData, Size);
	Info.m_UncompressedSize = Size;
	Info.m_pCompressedData = nullptr;
	Info.m_CompressedSize = 0;
	Info.m_CompressionLevel = CompressionLevel;

	return m_vDatas.size() - 1;
}

int CDataFileWriter::AddDataSwapped(size_t Size, const void *pData)
{
	dbg_assert(Size > 0 && pData != nullptr, "Data missing");
	dbg_assert(Size <= (size_t)std::numeric_limits<int>::max(), "Data too large");
	dbg_assert(Size % sizeof(int) == 0, "Invalid data boundary");

#if defined(CONF_ARCH_ENDIAN_BIG)
	void *pSwapped = malloc(Size); // temporary buffer that we use during compression
	mem_copy(pSwapped, pData, Size);
	swap_endian(pSwapped, sizeof(int), Size / sizeof(int));
	int Index = AddData(Size, pSwapped);
	free(pSwapped);
	return Index;
#else
	return AddData(Size, pData);
#endif
}

int CDataFileWriter::AddDataString(const char *pStr)
{
	dbg_assert(pStr != nullptr, "Data missing");

	if(pStr[0] == '\0')
		return -1;
	return AddData(str_length(pStr) + 1, pStr);
}

static int CompressionLevelToZlib(CDataFileWriter::ECompressionLevel CompressionLevel)
{
	switch(CompressionLevel)
	{
	case CDataFileWriter::COMPRESSION_DEFAULT:
		return Z_DEFAULT_COMPRESSION;
	case CDataFileWriter::COMPRESSION_BEST:
		return Z_BEST_COMPRESSION;
	default:
		dbg_assert(false, "CompressionLevel invalid");
		dbg_break();
	}
}

void CDataFileWriter::Finish()
{
	dbg_assert((bool)m_File, "File not open");

	// Compress data. This takes the majority of the time when saving a datafile,
	// so it's delayed until the end so it can be off-loaded to another thread.
	for(CDataInfo &DataInfo : m_vDatas)
	{
		unsigned long CompressedSize = compressBound(DataInfo.m_UncompressedSize);
		DataInfo.m_pCompressedData = malloc(CompressedSize);
		const int Result = compress2((Bytef *)DataInfo.m_pCompressedData, &CompressedSize, (Bytef *)DataInfo.m_pUncompressedData, DataInfo.m_UncompressedSize, CompressionLevelToZlib(DataInfo.m_CompressionLevel));
		DataInfo.m_CompressedSize = CompressedSize;
		free(DataInfo.m_pUncompressedData);
		DataInfo.m_pUncompressedData = nullptr;
		if(Result != Z_OK)
		{
			char aError[32];
			str_format(aError, sizeof(aError), "zlib compression error %d", Result);
			dbg_assert(false, aError);
		}
	}

	// Calculate total size of items
	size_t ItemSize = 0;
	for(const CItemInfo &ItemInfo : m_vItems)
	{
		ItemSize += ItemInfo.m_Size;
		ItemSize += sizeof(CDatafileItem);
	}

	// Calculate total size of data
	size_t DataSize = 0;
	for(const CDataInfo &DataInfo : m_vDatas)
		DataSize += DataInfo.m_CompressedSize;

	// Calculate complete file size
	const size_t TypesSize = m_ItemTypes.size() * sizeof(CDatafileItemType);
	const size_t HeaderSize = sizeof(CDatafileHeader);
	const size_t OffsetSize = (m_vItems.size() + m_vDatas.size() * 2) * sizeof(int); // ItemOffsets, DataOffsets, DataUncompressedSizes
	const size_t SwapSize = HeaderSize + TypesSize + OffsetSize + ItemSize;
	const size_t FileSize = SwapSize + DataSize;

	if(DEBUG)
		dbg_msg("datafile", "NumItemTypes=%" PRIzu " TypesSize=%" PRIzu " ItemSize=%" PRIzu " DataSize=%" PRIzu, m_ItemTypes.size(), TypesSize, ItemSize, DataSize);

	// This also ensures that SwapSize, ItemSize and DataSize are valid.
	dbg_assert(FileSize <= (size_t)std::numeric_limits<int>::max(), "File size too large");

	// Construct and write header
	{
		CDatafileHeader Header;
		Header.m_aId[0] = 'D';
		Header.m_aId[1] = 'A';
		Header.m_aId[2] = 'T';
		Header.m_aId[3] = 'A';
		Header.m_Version = 4;
		Header.m_Size = FileSize - Header.SizeOffset();
		Header.m_Swaplen = SwapSize - Header.SizeOffset();
		Header.m_NumItemTypes = m_ItemTypes.size();
		Header.m_NumItems = m_vItems.size();
		Header.m_NumRawData = m_vDatas.size();
		Header.m_ItemSize = ItemSize;
		Header.m_DataSize = DataSize;

#if defined(CONF_ARCH_ENDIAN_BIG)
		swap_endian(&Header, sizeof(int), sizeof(Header) / sizeof(int));
#endif
		io_write(m_File, &Header, sizeof(Header));
	}

	// Write item types
	int ItemCount = 0;
	for(const auto &[Type, ItemType] : m_ItemTypes)
	{
		dbg_assert(ItemType.m_Num > 0, "Invalid item type entry");

		CDatafileItemType Info;
		Info.m_Type = Type;
		Info.m_Start = ItemCount;
		Info.m_Num = ItemType.m_Num;

		if(DEBUG)
			dbg_msg("datafile", "writing item type. Type=%x Start=%d Num=%d", Info.m_Type, Info.m_Start, Info.m_Num);

#if defined(CONF_ARCH_ENDIAN_BIG)
		swap_endian(&Info, sizeof(int), sizeof(CDatafileItemType) / sizeof(int));
#endif
		io_write(m_File, &Info, sizeof(Info));
		ItemCount += ItemType.m_Num;
	}

	// Write item offsets sorted by type
	int ItemOffset = 0;
	for(const auto &[Type, ItemType] : m_ItemTypes)
	{
		// Write all items offsets of this type
		for(int ItemIndex = ItemType.m_First; ItemIndex != -1; ItemIndex = m_vItems[ItemIndex].m_Next)
		{
			if(DEBUG)
				dbg_msg("datafile", "writing item offset. Type=%d ItemIndex=%d ItemOffset=%d", Type, ItemIndex, ItemOffset);

			int Temp = ItemOffset;
#if defined(CONF_ARCH_ENDIAN_BIG)
			swap_endian(&Temp, sizeof(int), sizeof(Temp) / sizeof(int));
#endif
			io_write(m_File, &Temp, sizeof(Temp));
			ItemOffset += m_vItems[ItemIndex].m_Size + sizeof(CDatafileItem);
		}
	}

	// Write data offsets
	int DataOffset = 0, DataIndex = 0;
	for(const CDataInfo &DataInfo : m_vDatas)
	{
		if(DEBUG)
			dbg_msg("datafile", "writing data offset. DataIndex=%d DataOffset=%d", DataIndex, DataOffset);

		int Temp = DataOffset;
#if defined(CONF_ARCH_ENDIAN_BIG)
		swap_endian(&Temp, sizeof(int), sizeof(Temp) / sizeof(int));
#endif
		io_write(m_File, &Temp, sizeof(Temp));
		DataOffset += DataInfo.m_CompressedSize;
		++DataIndex;
	}

	// Write data uncompressed sizes
	DataIndex = 0;
	for(const CDataInfo &DataInfo : m_vDatas)
	{
		if(DEBUG)
			dbg_msg("datafile", "writing data uncompressed size. DataIndex=%d UncompressedSize=%d", DataIndex, DataInfo.m_UncompressedSize);

		int UncompressedSize = DataInfo.m_UncompressedSize;
#if defined(CONF_ARCH_ENDIAN_BIG)
		swap_endian(&UncompressedSize, sizeof(int), sizeof(UncompressedSize) / sizeof(int));
#endif
		io_write(m_File, &UncompressedSize, sizeof(UncompressedSize));
		++DataIndex;
	}

	// Write items sorted by type
	for(const auto &[Type, ItemType] : m_ItemTypes)
	{
		// Write all items of this type
		for(int ItemIndex = ItemType.m_First; ItemIndex != -1; ItemIndex = m_vItems[ItemIndex].m_Next)
		{
			CDatafileItem Item;
			Item.m_TypeAndId = (Type << 16) | m_vItems[ItemIndex].m_Id;
			Item.m_Size = m_vItems[ItemIndex].m_Size;

			if(DEBUG)
				dbg_msg("datafile", "writing item. Type=%x ItemIndex=%d Id=%d Size=%d", Type, ItemIndex, m_vItems[ItemIndex].m_Id, m_vItems[ItemIndex].m_Size);

#if defined(CONF_ARCH_ENDIAN_BIG)
			swap_endian(&Item, sizeof(int), sizeof(Item) / sizeof(int));
			if(m_vItems[ItemIndex].m_pData != nullptr)
				swap_endian(m_vItems[ItemIndex].m_pData, sizeof(int), m_vItems[ItemIndex].m_Size / sizeof(int));
#endif
			io_write(m_File, &Item, sizeof(Item));
			if(m_vItems[ItemIndex].m_pData != nullptr)
			{
				io_write(m_File, m_vItems[ItemIndex].m_pData, m_vItems[ItemIndex].m_Size);
				free(m_vItems[ItemIndex].m_pData);
				m_vItems[ItemIndex].m_pData = nullptr;
			}
		}
	}

	// Write data
	DataIndex = 0;
	for(CDataInfo &DataInfo : m_vDatas)
	{
		if(DEBUG)
			dbg_msg("datafile", "writing data. DataIndex=%d CompressedSize=%d", DataIndex, DataInfo.m_CompressedSize);

		io_write(m_File, DataInfo.m_pCompressedData, DataInfo.m_CompressedSize);
		free(DataInfo.m_pCompressedData);
		DataInfo.m_pCompressedData = nullptr;
		++DataIndex;
	}

	io_close(m_File);
	m_File = nullptr;
}
