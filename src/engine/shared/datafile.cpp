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
#include <unordered_set>

#include <zlib.h>

static constexpr int MAX_ITEM_TYPE = 0xFFFF;
static constexpr int MAX_ITEM_ID = 0xFFFF;
static constexpr int OFFSET_UUID_TYPE = 0x8000;

inline void SwapEndianInPlace(void *pObj, size_t Size)
{
#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(pObj, sizeof(int), Size / sizeof(int));
#endif
}

template<typename T>
inline void SwapEndianInPlace(T *pObj)
{
	static_assert(sizeof(T) % sizeof(int) == 0);
	SwapEndianInPlace(pObj, sizeof(T));
}

inline int SwapEndianInt(int Number)
{
	SwapEndianInPlace(&Number);
	return Number;
}

class CItemEx
{
public:
	int m_aUuid[sizeof(CUuid) / sizeof(int32_t)];

	static CItemEx FromUuid(CUuid Uuid)
	{
		CItemEx Result;
		for(size_t i = 0; i < std::size(Result.m_aUuid); i++)
		{
			Result.m_aUuid[i] = bytes_be_to_uint(&Uuid.m_aData[i * sizeof(int32_t)]);
		}
		return Result;
	}

	CUuid ToUuid() const
	{
		CUuid Result;
		for(size_t i = 0; i < std::size(m_aUuid); i++)
		{
			uint_to_bytes_be(&Result.m_aData[i * sizeof(int32_t)], m_aUuid[i]);
		}
		return Result;
	}
};

class CDatafileItemType
{
public:
	int m_Type;
	int m_Start;
	int m_Num;
};

class CDatafileItem
{
public:
	unsigned m_TypeAndId;
	int m_Size;

	int Type() const
	{
		return (m_TypeAndId >> 16u) & MAX_ITEM_TYPE;
	}

	int Id() const
	{
		return m_TypeAndId & MAX_ITEM_ID;
	}
};

class CDatafileHeader
{
public:
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

class CDatafileInfo
{
public:
	CDatafileItemType *m_pItemTypes;
	int *m_pItemOffsets;
	int *m_pDataOffsets;
	int *m_pDataSizes;

	char *m_pItemStart;
	char *m_pDataStart;
};

class CDatafile
{
public:
	IOHANDLE m_File;
	unsigned m_FileSize;
	SHA256_DIGEST m_Sha256;
	unsigned m_Crc;
	CDatafileInfo m_Info;
	CDatafileHeader m_Header;
	int m_DataStartOffset;
	void **m_ppDataPtrs;
	int *m_pDataSizes;
	char *m_pData;

	int GetFileDataSize(int Index) const
	{
		dbg_assert(Index >= 0 && Index < m_Header.m_NumRawData, "Index invalid: %d", Index);

		if(Index == m_Header.m_NumRawData - 1)
		{
			return m_Header.m_DataSize - m_Info.m_pDataOffsets[Index];
		}

		return m_Info.m_pDataOffsets[Index + 1] - m_Info.m_pDataOffsets[Index];
	}

	int GetDataSize(int Index) const
	{
		// Invalid data indices may appear in map items
		if(Index < 0 || Index >= m_Header.m_NumRawData)
		{
			return 0;
		}

		if(m_ppDataPtrs[Index] == nullptr)
		{
			if(m_Info.m_pDataSizes != nullptr)
			{
				return m_Info.m_pDataSizes[Index];
			}
			else
			{
				return GetFileDataSize(Index);
			}
		}

		const int Size = m_pDataSizes[Index];
		if(Size < 0)
		{
			return 0; // summarize all errors as zero size
		}
		return Size;
	}

	void *GetData(int Index, bool Swap) const
	{
		// Invalid data indices may appear in map items
		if(Index < 0 || Index >= m_Header.m_NumRawData)
		{
			return nullptr;
		}

		// Data already loaded
		if(m_ppDataPtrs[Index] != nullptr)
		{
			return m_ppDataPtrs[Index];
		}

		// Don't try to load the data again if it previously failed
		if(m_pDataSizes[Index] < 0)
		{
			return nullptr;
		}

		const unsigned DataSize = GetFileDataSize(Index);
		if(m_Info.m_pDataSizes != nullptr)
		{
			// v4 has compressed data
			const unsigned OriginalUncompressedSize = m_Info.m_pDataSizes[Index];
			log_trace("datafile", "loading data. index=%d size=%d uncompressed=%d", Index, DataSize, OriginalUncompressedSize);
			if(OriginalUncompressedSize == 0)
			{
				log_error("datafile", "data size invalid. data will be ignored. index=%d size=%d uncompressed=%d", Index, DataSize, OriginalUncompressedSize);
				m_ppDataPtrs[Index] = nullptr;
				m_pDataSizes[Index] = -1;
				return nullptr;
			}

			// read the compressed data
			void *pCompressedData = malloc(DataSize);
			if(pCompressedData == nullptr)
			{
				log_error("datafile", "out of memory. could not allocate memory for compressed data. index=%d size=%d", Index, DataSize);
				m_ppDataPtrs[Index] = nullptr;
				m_pDataSizes[Index] = -1;
				return nullptr;
			}
			unsigned ActualDataSize = 0;
			if(io_seek(m_File, m_DataStartOffset + m_Info.m_pDataOffsets[Index], IOSEEK_START) == 0)
			{
				ActualDataSize = io_read(m_File, pCompressedData, DataSize);
			}
			if(DataSize != ActualDataSize)
			{
				log_error("datafile", "truncation error. could not read all compressed data. index=%d wanted=%d got=%d", Index, DataSize, ActualDataSize);
				free(pCompressedData);
				m_ppDataPtrs[Index] = nullptr;
				m_pDataSizes[Index] = -1;
				return nullptr;
			}

			// decompress the data
			m_ppDataPtrs[Index] = static_cast<char *>(malloc(OriginalUncompressedSize));
			if(m_ppDataPtrs[Index] == nullptr)
			{
				free(pCompressedData);
				log_error("datafile", "out of memory. could not allocate memory for uncompressed data. index=%d size=%d", Index, OriginalUncompressedSize);
				m_pDataSizes[Index] = -1;
				return nullptr;
			}
			unsigned long UncompressedSize = OriginalUncompressedSize;
			const int Result = uncompress(static_cast<Bytef *>(m_ppDataPtrs[Index]), &UncompressedSize, static_cast<Bytef *>(pCompressedData), DataSize);
			free(pCompressedData);
			if(Result != Z_OK || UncompressedSize != OriginalUncompressedSize)
			{
				log_error("datafile", "failed to uncompress data. index=%d result=%d wanted=%d got=%ld", Index, Result, OriginalUncompressedSize, UncompressedSize);
				free(m_ppDataPtrs[Index]);
				m_ppDataPtrs[Index] = nullptr;
				m_pDataSizes[Index] = -1;
				return nullptr;
			}
			m_pDataSizes[Index] = OriginalUncompressedSize;
		}
		else
		{
			log_trace("datafile", "loading data. index=%d size=%d", Index, DataSize);
			m_ppDataPtrs[Index] = malloc(DataSize);
			if(m_ppDataPtrs[Index] == nullptr)
			{
				log_error("datafile", "out of memory. could not allocate memory for uncompressed data. index=%d size=%d", Index, DataSize);
				m_pDataSizes[Index] = -1;
				return nullptr;
			}
			unsigned ActualDataSize = 0;
			if(io_seek(m_File, m_DataStartOffset + m_Info.m_pDataOffsets[Index], IOSEEK_START) == 0)
			{
				ActualDataSize = io_read(m_File, m_ppDataPtrs[Index], DataSize);
			}
			if(DataSize != ActualDataSize)
			{
				log_error("datafile", "truncation error. could not read all uncompressed data. index=%d wanted=%d got=%d", Index, DataSize, ActualDataSize);
				free(m_ppDataPtrs[Index]);
				m_ppDataPtrs[Index] = nullptr;
				m_pDataSizes[Index] = -1;
				return nullptr;
			}
			m_pDataSizes[Index] = DataSize;
		}
		if(Swap)
		{
			SwapEndianInPlace(m_ppDataPtrs[Index], m_pDataSizes[Index]);
		}
		return m_ppDataPtrs[Index];
	}

	int GetFileItemSize(int Index) const
	{
		dbg_assert(Index >= 0 && Index < m_Header.m_NumItems, "Index invalid: %d", Index);

		if(Index == m_Header.m_NumItems - 1)
		{
			return m_Header.m_ItemSize - m_Info.m_pItemOffsets[Index];
		}

		return m_Info.m_pItemOffsets[Index + 1] - m_Info.m_pItemOffsets[Index];
	}

	int GetItemSize(int Index) const
	{
		return GetFileItemSize(Index) - sizeof(CDatafileItem);
	}

	CDatafileItem *GetItem(int Index) const
	{
		dbg_assert(Index >= 0 && Index < m_Header.m_NumItems, "Index invalid: %d", Index);

		return static_cast<CDatafileItem *>(static_cast<void *>(m_Info.m_pItemStart + m_Info.m_pItemOffsets[Index]));
	}

	bool Validate() const
	{
#define Check(Test, ErrorMessage, ...) \
	do \
	{ \
		if(!(Test)) \
		{ \
			log_error("datafile", "invalid file information: " ErrorMessage, ##__VA_ARGS__); \
			return false; \
		} \
	} while(false)

		// validate item types
		int64_t CountedItems = 0;
		std::unordered_set<int> UsedItemTypes;
		for(int Index = 0; Index < m_Header.m_NumItemTypes; Index++)
		{
			const CDatafileItemType &ItemType = m_Info.m_pItemTypes[Index];
			Check(ItemType.m_Type >= 0 && ItemType.m_Type <= MAX_ITEM_TYPE, "item type has invalid type. index=%d type=%d", Index, ItemType.m_Type);
			const auto [_, Inserted] = UsedItemTypes.insert(ItemType.m_Type);
			Check(Inserted, "item type has duplicate type. index=%d type=%d", Index, ItemType.m_Type);
			Check(ItemType.m_Num > 0, "item type has invalid number of items. index=%d type=%d num=%d", Index, ItemType.m_Type, ItemType.m_Num);
			Check(ItemType.m_Start == CountedItems, "item type has invalid start. index=%d type=%d start=%d", Index, ItemType.m_Type, ItemType.m_Start);
			CountedItems += ItemType.m_Num;
			if(CountedItems > m_Header.m_NumItems)
			{
				break;
			}
		}
		Check(CountedItems == m_Header.m_NumItems, "mismatched number of items in item types. counted=%" PRId64 " header=%d", CountedItems, m_Header.m_NumItems);

		// validate item offsets
		int PrevItemOffset = -1;
		for(int Index = 0; Index < m_Header.m_NumItems; Index++)
		{
			const int Offset = m_Info.m_pItemOffsets[Index];
			if(Index == 0)
			{
				Check(Offset == 0, "first item offset is not zero. offset=%d", Offset);
			}
			else
			{
				Check(Offset > PrevItemOffset, "item offset not greater than previous. index=%d offset=%d previous=%d", Index, Offset, PrevItemOffset);
			}
			Check(Offset < m_Header.m_ItemSize, "item offset larger than total item size. index=%d offset=%d total=%d", Index, Offset, m_Header.m_ItemSize);
			PrevItemOffset = Offset;
		}

		// validate item sizes, types and IDs
		int64_t TotalItemSize = 0;
		for(int TypeIndex = 0; TypeIndex < m_Header.m_NumItemTypes; TypeIndex++)
		{
			std::unordered_set<int> UsedItemIds;
			const CDatafileItemType &ItemType = m_Info.m_pItemTypes[TypeIndex];
			for(int ItemIndex = ItemType.m_Start; ItemIndex < ItemType.m_Start + ItemType.m_Num; ItemIndex++)
			{
				const int FileItemSize = GetFileItemSize(ItemIndex);
				Check(FileItemSize >= (int)sizeof(CDatafileItem), "map item too small for header. type_index=%d item_index=%d size=%d header=%d", TypeIndex, ItemIndex, FileItemSize, (int)sizeof(CDatafileItem));
				const CDatafileItem *pItem = GetItem(ItemIndex);
				Check(pItem->Type() == ItemType.m_Type, "mismatched item type. type_index=%d item_index=%d type=%d expected=%d", TypeIndex, ItemIndex, pItem->Type(), ItemType.m_Type);
				// Many old maps contain duplicate map items of type ITEMTYPE_EX due to a bug in DDNet tools.
				if(pItem->Type() != ITEMTYPE_EX)
				{
					const auto [_, Inserted] = UsedItemIds.insert(pItem->Id());
					Check(Inserted, "map item has duplicate ID. type_index=%d item_index=%d type=%d ID=%d", TypeIndex, ItemIndex, pItem->Type(), pItem->Id());
				}
				Check(pItem->m_Size >= 0, "map item size invalid. type_index=%d item_index=%d size=%d", TypeIndex, ItemIndex, pItem->m_Size);
				Check(pItem->m_Size % sizeof(int) == 0, "map item size not integer aligned. type_index=%d item_index=%d size=%d", TypeIndex, ItemIndex, pItem->m_Size);
				Check(pItem->m_Size == GetItemSize(ItemIndex), "map item size does not match file. type_index=%d item_index=%d size=%d file_size=%d", TypeIndex, ItemIndex, pItem->m_Size, GetItemSize(ItemIndex));
				TotalItemSize += FileItemSize;
				if(TotalItemSize > m_Header.m_ItemSize)
				{
					break;
				}
			}
		}
		Check(TotalItemSize == m_Header.m_ItemSize, "mismatched total item size. expected=%" PRId64 " header=%d", TotalItemSize, m_Header.m_ItemSize);

		// validate data offsets
		int PrevDataOffset = -1;
		for(int Index = 0; Index < m_Header.m_NumRawData; Index++)
		{
			const int Offset = m_Info.m_pDataOffsets[Index];
			if(Index == 0)
			{
				Check(Offset == 0, "first data offset must be zero. offset=%d", Offset);
			}
			else
			{
				Check(Offset > PrevDataOffset, "data offset not greater than previous. index=%d offset=%d previous=%d", Index, Offset, PrevDataOffset);
			}
			Check(Offset < m_Header.m_DataSize, "data offset larger than total data size. index=%d offset=%d total=%d", Index, Offset, m_Header.m_DataSize);
			PrevDataOffset = Offset;
		}

		// validate data sizes
		if(m_Info.m_pDataSizes != nullptr)
		{
			for(int Index = 0; Index < m_Header.m_NumRawData; Index++)
			{
				const int Size = m_Info.m_pDataSizes[Index];
				Check(Size >= 0, "data size invalid. index=%d size=%d", Index, Size);
				if(Size == 0)
				{
					// Data of size zero is not allowed, but due to existing maps with this quirk we instead allow
					// the file to be loaded and fail loading the data in the GetData function if the size is zero.
					log_warn("datafile", "invalid file information: data size invalid. index=%d size=%d", Index, Size);
				}
			}
		}

		return true;
#undef Check
	}
};

CDataFileReader::~CDataFileReader()
{
	Close();
}

CDataFileReader &CDataFileReader::operator=(CDataFileReader &&Other)
{
	m_pDataFile = Other.m_pDataFile;
	Other.m_pDataFile = nullptr;
	return *this;
}

bool CDataFileReader::Open(class IStorage *pStorage, const char *pFilename, int StorageType)
{
	dbg_assert(m_pDataFile == nullptr, "File already open");

	log_trace("datafile", "loading '%s'", pFilename);

	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType);
	if(!File)
	{
		log_error("datafile", "failed to open file '%s' for reading", pFilename);
		return false;
	}

	// determine size and hashes of the file and store them
	int64_t FileSize = 0;
	unsigned Crc = 0;
	SHA256_DIGEST Sha256;
	{
		SHA256_CTX Sha256Ctxt;
		sha256_init(&Sha256Ctxt);
		unsigned char aBuffer[64 * 1024];
		while(true)
		{
			const unsigned Bytes = io_read(File, aBuffer, sizeof(aBuffer));
			if(Bytes == 0)
				break;
			FileSize += Bytes;
			Crc = crc32(Crc, aBuffer, Bytes);
			sha256_update(&Sha256Ctxt, aBuffer, Bytes);
		}
		Sha256 = sha256_finish(&Sha256Ctxt);
		if(io_seek(File, 0, IOSEEK_START) != 0)
		{
			io_close(File);
			log_error("datafile", "could not seek to start after calculating hashes");
			return false;
		}
	}

	// read header
	CDatafileHeader Header;
	if(io_read(File, &Header, sizeof(Header)) != sizeof(Header))
	{
		io_close(File);
		log_error("datafile", "could not read file header. file truncated or not a datafile.");
		return false;
	}

	// check header magic
	if((Header.m_aId[0] != 'A' || Header.m_aId[1] != 'T' || Header.m_aId[2] != 'A' || Header.m_aId[3] != 'D') &&
		(Header.m_aId[0] != 'D' || Header.m_aId[1] != 'A' || Header.m_aId[2] != 'T' || Header.m_aId[3] != 'A'))
	{
		io_close(File);
		log_error("datafile", "wrong header magic. magic=%x%x%x%x", Header.m_aId[0], Header.m_aId[1], Header.m_aId[2], Header.m_aId[3]);
		return false;
	}

	SwapEndianInPlace(&Header);

	// check header version
	if(Header.m_Version != 3 && Header.m_Version != 4)
	{
		io_close(File);
		log_error("datafile", "unsupported header version. version=%d", Header.m_Version);
		return false;
	}

	// validate header information
	if(Header.m_NumItemTypes < 0 ||
		Header.m_NumItemTypes > MAX_ITEM_TYPE + 1 ||
		Header.m_NumItems < 0 ||
		Header.m_NumRawData < 0 ||
		Header.m_ItemSize < 0 ||
		Header.m_ItemSize % sizeof(int) != 0 ||
		Header.m_DataSize < 0)
	{
		io_close(File);
		log_error("datafile", "invalid header information. num_types=%d num_items=%d num_data=%d item_size=%d data_size=%d",
			Header.m_NumItemTypes, Header.m_NumItems, Header.m_NumRawData, Header.m_ItemSize, Header.m_DataSize);
		return false;
	}

	// calculate and validate sizes
	int64_t Size = 0;
	Size += (int64_t)Header.m_NumItemTypes * sizeof(CDatafileItemType);
	Size += (int64_t)Header.m_NumItems * sizeof(int);
	Size += (int64_t)Header.m_NumRawData * sizeof(int);
	int64_t SizeFix = 0;
	if(Header.m_Version == 4) // v4 has uncompressed data sizes as well
	{
		// The size of the uncompressed data sizes was not included in
		// Header.m_Size and Header.m_Swaplen of version 4 maps prior
		// to commit 3dd1ea0d8f6cb442ac41bd223279f41d1ed1b2bb. We also
		// support loading maps created prior to this commit by fixing
		// the sizes transparently when loading.
		SizeFix = (int64_t)Header.m_NumRawData * sizeof(int);
		Size += SizeFix;
	}
	Size += Header.m_ItemSize;

	if((int64_t)sizeof(Header) + Size + (int64_t)Header.m_DataSize != FileSize)
	{
		io_close(File);
		log_error("datafile", "invalid header data size or truncated file. data_size=%" PRId64 " file_size=%" PRId64, Header.m_DataSize, FileSize);
		return false;
	}

	const int64_t HeaderFileSize = (int64_t)Header.m_Size + Header.SizeOffset();
	if(HeaderFileSize != FileSize)
	{
		if(SizeFix != 0 && HeaderFileSize + SizeFix == FileSize)
		{
			log_warn("datafile", "fixing invalid header size. size=%d fix=+%" PRId64, Header.m_Size, SizeFix);
			Header.m_Size += SizeFix;
		}
		else
		{
			io_close(File);
			log_error("datafile", "invalid header size or truncated file. size=%" PRId64 " actual=%" PRId64, HeaderFileSize, FileSize);
			return false;
		}
	}

	const int64_t HeaderSwaplen = (int64_t)Header.m_Swaplen + Header.SizeOffset();
	const int64_t FileSizeSwaplen = FileSize - Header.m_DataSize;
	if(HeaderSwaplen != FileSizeSwaplen)
	{
		if(Header.m_Swaplen % sizeof(int) == 0 && SizeFix != 0 && HeaderSwaplen + SizeFix == FileSizeSwaplen)
		{
			log_warn("datafile", "fixing invalid header swaplen. swaplen=%d fix=+%d", Header.m_Swaplen, SizeFix);
			Header.m_Swaplen += SizeFix;
		}
		else
		{
			io_close(File);
			log_error("datafile", "invalid header swaplen or truncated file. swaplen=%" PRId64 " actual=%" PRId64, HeaderSwaplen, FileSizeSwaplen);
			return false;
		}
	}

	constexpr int64_t MaxAllocSize = (int64_t)2 * 1024 * 1024 * 1024;
	int64_t AllocSize = Size;
	AllocSize += sizeof(CDatafile); // add space for info structure
	AllocSize += (int64_t)Header.m_NumRawData * sizeof(void *); // add space for data pointers
	AllocSize += (int64_t)Header.m_NumRawData * sizeof(int); // add space for data sizes
	if(AllocSize > MaxAllocSize)
	{
		io_close(File);
		log_error("datafile", "file too large. alloc_size=%" PRId64 " max=%" PRId64, AllocSize, MaxAllocSize);
		return false;
	}

	CDatafile *pTmpDataFile = static_cast<CDatafile *>(malloc(AllocSize));
	if(pTmpDataFile == nullptr)
	{
		io_close(File);
		log_error("datafile", "out of memory. could not allocate memory for datafile. alloc_size=%" PRId64, AllocSize);
		return false;
	}
	pTmpDataFile->m_Header = Header;
	pTmpDataFile->m_DataStartOffset = sizeof(CDatafileHeader) + Size;
	pTmpDataFile->m_ppDataPtrs = (void **)(pTmpDataFile + 1);
	pTmpDataFile->m_pDataSizes = (int *)(pTmpDataFile->m_ppDataPtrs + Header.m_NumRawData);
	pTmpDataFile->m_pData = (char *)(pTmpDataFile->m_pDataSizes + Header.m_NumRawData);
	pTmpDataFile->m_File = File;
	pTmpDataFile->m_FileSize = FileSize;
	pTmpDataFile->m_Sha256 = Sha256;
	pTmpDataFile->m_Crc = Crc;

	// clear the data pointers and sizes
	mem_zero(pTmpDataFile->m_ppDataPtrs, Header.m_NumRawData * sizeof(void *));
	mem_zero(pTmpDataFile->m_pDataSizes, Header.m_NumRawData * sizeof(int));

	// read types, offsets, sizes and item data
	const unsigned ReadSize = io_read(pTmpDataFile->m_File, pTmpDataFile->m_pData, Size);
	if((int64_t)ReadSize != Size)
	{
		io_close(pTmpDataFile->m_File);
		free(pTmpDataFile);
		log_error("datafile", "truncation error. could not read all item data. wanted=%" PRIzu " got=%d", Size, ReadSize);
		return false;
	}

	// The swap len also includes the size of the header (without the size offset), but the header was already swapped above.
	const int64_t DataSwapLen = pTmpDataFile->m_Header.m_Swaplen - (int)(sizeof(Header) - Header.SizeOffset());
	dbg_assert(DataSwapLen == Size, "Swap len and file size mismatch");
	SwapEndianInPlace(pTmpDataFile->m_pData, DataSwapLen);

	pTmpDataFile->m_Info.m_pItemTypes = (CDatafileItemType *)pTmpDataFile->m_pData;
	pTmpDataFile->m_Info.m_pItemOffsets = (int *)&pTmpDataFile->m_Info.m_pItemTypes[pTmpDataFile->m_Header.m_NumItemTypes];
	pTmpDataFile->m_Info.m_pDataOffsets = &pTmpDataFile->m_Info.m_pItemOffsets[pTmpDataFile->m_Header.m_NumItems];
	if(pTmpDataFile->m_Header.m_Version == 4) // v4 has uncompressed data sizes as well
	{
		pTmpDataFile->m_Info.m_pDataSizes = &pTmpDataFile->m_Info.m_pDataOffsets[pTmpDataFile->m_Header.m_NumRawData];
		pTmpDataFile->m_Info.m_pItemStart = (char *)&pTmpDataFile->m_Info.m_pDataSizes[pTmpDataFile->m_Header.m_NumRawData];
	}
	else
	{
		pTmpDataFile->m_Info.m_pDataSizes = nullptr;
		pTmpDataFile->m_Info.m_pItemStart = (char *)&pTmpDataFile->m_Info.m_pDataOffsets[pTmpDataFile->m_Header.m_NumRawData];
	}
	pTmpDataFile->m_Info.m_pDataStart = pTmpDataFile->m_Info.m_pItemStart + pTmpDataFile->m_Header.m_ItemSize;

	if(!pTmpDataFile->Validate())
	{
		io_close(pTmpDataFile->m_File);
		free(pTmpDataFile);
		return false;
	}

	m_pDataFile = pTmpDataFile;
	log_trace("datafile", "loading done. datafile='%s'", pFilename);

	return true;
}

void CDataFileReader::Close()
{
	if(!m_pDataFile)
	{
		return;
	}

	for(int i = 0; i < m_pDataFile->m_Header.m_NumRawData; i++)
	{
		free(m_pDataFile->m_ppDataPtrs[i]);
	}

	io_close(m_pDataFile->m_File);
	free(m_pDataFile);
	m_pDataFile = nullptr;
}

bool CDataFileReader::IsOpen() const
{
	return m_pDataFile != nullptr;
}

IOHANDLE CDataFileReader::File() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_File;
}

int CDataFileReader::GetDataSize(int Index) const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->GetDataSize(Index);
}

void *CDataFileReader::GetData(int Index)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->GetData(Index, false);
}

void *CDataFileReader::GetDataSwapped(int Index)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->GetData(Index, true);
}

const char *CDataFileReader::GetDataString(int Index)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	if(Index == -1)
	{
		return "";
	}

	const int DataSize = GetDataSize(Index);
	if(!DataSize)
	{
		return nullptr;
	}

	const char *pData = static_cast<const char *>(GetData(Index));
	if(pData == nullptr || mem_has_null(pData, DataSize - 1) || pData[DataSize - 1] != '\0' || !str_utf8_check(pData))
	{
		return nullptr;
	}
	return pData;
}

void CDataFileReader::ReplaceData(int Index, char *pData, size_t Size)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");
	dbg_assert(Index >= 0 && Index < m_pDataFile->m_Header.m_NumRawData, "Index invalid: %d", Index);

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

int CDataFileReader::NumData() const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->m_Header.m_NumRawData;
}

int CDataFileReader::GetItemSize(int Index) const
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	return m_pDataFile->GetItemSize(Index);
}

int CDataFileReader::GetExternalItemType(int InternalType, CUuid *pUuid)
{
	if(InternalType <= OFFSET_UUID_TYPE || InternalType == ITEMTYPE_EX)
	{
		if(pUuid)
		{
			*pUuid = UUID_ZEROED;
		}
		return InternalType;
	}

	const int TypeIndex = FindItemIndex(ITEMTYPE_EX, InternalType);
	if(TypeIndex < 0 || GetItemSize(TypeIndex) < (int)sizeof(CItemEx))
	{
		if(pUuid)
		{
			*pUuid = UUID_ZEROED;
		}
		return InternalType;
	}

	const CItemEx *pItemEx = static_cast<const CItemEx *>(GetItem(TypeIndex));
	const CUuid Uuid = pItemEx->ToUuid();
	if(pUuid)
	{
		*pUuid = Uuid;
	}
	// Propagate UUID_UNKNOWN, it doesn't hurt.
	return g_UuidManager.LookupUuid(Uuid);
}

int CDataFileReader::GetInternalItemType(int ExternalType)
{
	if(ExternalType < OFFSET_UUID)
	{
		return ExternalType;
	}

	const CUuid Uuid = g_UuidManager.GetUuid(ExternalType);
	int Start, Num;
	GetType(ITEMTYPE_EX, &Start, &Num);
	for(int Index = Start; Index < Start + Num; Index++)
	{
		if(GetItemSize(Index) < (int)sizeof(CItemEx))
		{
			continue;
		}
		int Id;
		if(Uuid == static_cast<const CItemEx *>(GetItem(Index, nullptr, &Id))->ToUuid())
		{
			return Id;
		}
	}
	return -1;
}

void *CDataFileReader::GetItem(int Index, int *pType, int *pId, CUuid *pUuid)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	CDatafileItem *pItem = m_pDataFile->GetItem(Index);
	const int ExternalType = GetExternalItemType(pItem->Type(), pUuid);
	if(pType)
	{
		*pType = ExternalType;
	}
	if(pId)
	{
		*pId = pItem->Id();
	}
	return static_cast<void *>(pItem + 1);
}

void CDataFileReader::GetType(int Type, int *pStart, int *pNum)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	*pStart = 0;
	*pNum = 0;

	const int InternalType = GetInternalItemType(Type);
	for(int Index = 0; Index < m_pDataFile->m_Header.m_NumItemTypes; Index++)
	{
		const CDatafileItemType &ItemType = m_pDataFile->m_Info.m_pItemTypes[Index];
		if(ItemType.m_Type == InternalType)
		{
			*pStart = ItemType.m_Start;
			*pNum = ItemType.m_Num;
			return;
		}
	}
}

int CDataFileReader::FindItemIndex(int Type, int Id)
{
	dbg_assert(m_pDataFile != nullptr, "File not open");

	int Start, Num;
	GetType(Type, &Start, &Num);
	for(int Index = Start; Index < Start + Num; Index++)
	{
		const CDatafileItem *pItem = m_pDataFile->GetItem(Index);
		if(pItem->Id() == Id)
		{
			return Index;
		}
	}
	return -1;
}

void *CDataFileReader::FindItem(int Type, int Id)
{
	const int Index = FindItemIndex(Type, Id);
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

	return m_pDataFile->m_FileSize;
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
			{
				return Index;
			}
			++Index;
		}
	}
	else
	{
		for(const auto &ExtendedItemType : m_vExtendedItemTypes)
		{
			if(ExtendedItemType.m_Type == Type)
			{
				return Index;
			}
			++Index;
		}
	}

	// Type not found, add it.
	const CUuid Uuid = Type == -1 ? *pUuid : g_UuidManager.GetUuid(Type);
	CExtendedItemType ExtendedType;
	ExtendedType.m_Type = Type;
	ExtendedType.m_Uuid = Uuid;
	m_vExtendedItemTypes.emplace_back(ExtendedType);

	const CItemEx ItemEx = CItemEx::FromUuid(Uuid);
	AddItem(ITEMTYPE_EX, GetTypeFromIndex(Index), sizeof(ItemEx), &ItemEx);
	return Index;
}

int CDataFileWriter::AddItem(int Type, int Id, size_t Size, const void *pData, const CUuid *pUuid)
{
	dbg_assert((Type >= 0 && Type <= MAX_ITEM_TYPE) || Type >= OFFSET_UUID || (Type == -1 && pUuid != nullptr), "Invalid type: %d", Type);
	dbg_assert(Id >= 0 && Id <= MAX_ITEM_ID, "Invalid ID: %d", Id);
	dbg_assert(Size == 0 || pData != nullptr, "Data missing"); // Items without data are allowed
	dbg_assert(Size <= (size_t)std::numeric_limits<int>::max(), "Data too large");
	dbg_assert(Size % sizeof(int) == 0, "Invalid data boundary");
	dbg_assert(m_vItems.size() < (size_t)std::numeric_limits<int>::max(), "Too many items");

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
	{
		Info.m_pData = nullptr;
	}

	// link
	CItemTypeInfo &ItemType = m_ItemTypes[Type];
	Info.m_Prev = ItemType.m_Last;
	Info.m_Next = -1;

	if(ItemType.m_Last != -1)
	{
		m_vItems[ItemType.m_Last].m_Next = NumItems;
	}
	ItemType.m_Last = NumItems;

	if(ItemType.m_First == -1)
	{
		ItemType.m_First = NumItems;
	}

	ItemType.m_Num++;
	return NumItems;
}

int CDataFileWriter::AddData(size_t Size, const void *pData, ECompressionLevel CompressionLevel)
{
	dbg_assert(Size > 0 && pData != nullptr, "Data missing");
	dbg_assert(Size <= (size_t)std::numeric_limits<int>::max(), "Data too large");
	dbg_assert(m_vDatas.size() < (size_t)std::numeric_limits<int>::max(), "Too many data");

	CDataInfo Info;
	Info.m_pUncompressedData = malloc(Size);
	mem_copy(Info.m_pUncompressedData, pData, Size);
	Info.m_UncompressedSize = Size;
	Info.m_pCompressedData = nullptr;
	Info.m_CompressedSize = 0;
	Info.m_CompressionLevel = CompressionLevel;
	m_vDatas.emplace_back(Info);

	return m_vDatas.size() - 1;
}

int CDataFileWriter::AddDataSwapped(size_t Size, const void *pData)
{
	dbg_assert(Size > 0 && pData != nullptr, "Data missing");
	dbg_assert(Size <= (size_t)std::numeric_limits<int>::max(), "Data too large");
	dbg_assert(m_vDatas.size() < (size_t)std::numeric_limits<int>::max(), "Too many data");
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
	{
		return -1;
	}
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
		const int Result = compress2(static_cast<Bytef *>(DataInfo.m_pCompressedData), &CompressedSize, static_cast<Bytef *>(DataInfo.m_pUncompressedData), DataInfo.m_UncompressedSize, CompressionLevelToZlib(DataInfo.m_CompressionLevel));
		DataInfo.m_CompressedSize = CompressedSize;
		free(DataInfo.m_pUncompressedData);
		DataInfo.m_pUncompressedData = nullptr;
		dbg_assert(Result == Z_OK, "datafile zlib compression failed with error %d", Result);
	}

	// Calculate total size of items
	int64_t ItemSize = 0;
	for(const CItemInfo &ItemInfo : m_vItems)
	{
		ItemSize += ItemInfo.m_Size;
		ItemSize += sizeof(CDatafileItem);
	}

	// Calculate total size of data
	int64_t DataSize = 0;
	for(const CDataInfo &DataInfo : m_vDatas)
	{
		DataSize += DataInfo.m_CompressedSize;
	}

	// Calculate complete file size
	const int64_t TypesSize = m_ItemTypes.size() * sizeof(CDatafileItemType);
	const int64_t HeaderSize = sizeof(CDatafileHeader);
	const int64_t OffsetSize = (m_vItems.size() + m_vDatas.size() * 2) * sizeof(int); // ItemOffsets, DataOffsets, DataUncompressedSizes
	const int64_t SwapSize = HeaderSize + TypesSize + OffsetSize + ItemSize;
	const int64_t FileSize = SwapSize + DataSize;

	// This also ensures that SwapSize, ItemSize and DataSize are valid.
	dbg_assert(FileSize <= (int64_t)std::numeric_limits<int>::max(), "File size too large");

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

		SwapEndianInPlace(&Header);
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

		SwapEndianInPlace(&Info);
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
			const int ItemOffsetWrite = SwapEndianInt(ItemOffset);
			io_write(m_File, &ItemOffsetWrite, sizeof(ItemOffsetWrite));
			ItemOffset += m_vItems[ItemIndex].m_Size + sizeof(CDatafileItem);
		}
	}

	// Write data offsets
	int DataOffset = 0;
	for(const CDataInfo &DataInfo : m_vDatas)
	{
		const int DataOffsetWrite = SwapEndianInt(DataOffset);
		io_write(m_File, &DataOffsetWrite, sizeof(DataOffsetWrite));
		DataOffset += DataInfo.m_CompressedSize;
	}

	// Write data uncompressed sizes
	for(const CDataInfo &DataInfo : m_vDatas)
	{
		const int UncompressedSizeWrite = SwapEndianInt(DataInfo.m_UncompressedSize);
		io_write(m_File, &UncompressedSizeWrite, sizeof(UncompressedSizeWrite));
	}

	// Write items sorted by type
	for(const auto &[Type, ItemType] : m_ItemTypes)
	{
		// Write all items of this type
		for(int ItemIndex = ItemType.m_First; ItemIndex != -1; ItemIndex = m_vItems[ItemIndex].m_Next)
		{
			CDatafileItem Item;
			Item.m_TypeAndId = ((unsigned)Type << 16u) | (unsigned)m_vItems[ItemIndex].m_Id;
			Item.m_Size = m_vItems[ItemIndex].m_Size;

			SwapEndianInPlace(&Item);
			io_write(m_File, &Item, sizeof(Item));

			if(m_vItems[ItemIndex].m_pData != nullptr)
			{
				SwapEndianInPlace(m_vItems[ItemIndex].m_pData, m_vItems[ItemIndex].m_Size);
				io_write(m_File, m_vItems[ItemIndex].m_pData, m_vItems[ItemIndex].m_Size);
				free(m_vItems[ItemIndex].m_pData);
				m_vItems[ItemIndex].m_pData = nullptr;
			}
		}
	}

	// Write data
	for(CDataInfo &DataInfo : m_vDatas)
	{
		io_write(m_File, DataInfo.m_pCompressedData, DataInfo.m_CompressedSize);
		free(DataInfo.m_pCompressedData);
		DataInfo.m_pCompressedData = nullptr;
	}

	io_close(m_File);
	m_File = nullptr;
}
