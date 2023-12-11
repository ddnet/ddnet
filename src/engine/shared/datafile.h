/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_DATAFILE_H
#define ENGINE_SHARED_DATAFILE_H

#include <engine/storage.h>

#include <base/hash.h>
#include <base/types.h>

#include <array>
#include <vector>

#include <zlib.h>

enum
{
	ITEMTYPE_EX = 0xffff,
};

// raw datafile access
class CDataFileReader
{
	struct CDatafile *m_pDataFile;
	void *GetDataImpl(int Index, bool Swap);
	int GetFileDataSize(int Index) const;

	int GetExternalItemType(int InternalType);
	int GetInternalItemType(int ExternalType);

public:
	CDataFileReader() :
		m_pDataFile(nullptr) {}
	~CDataFileReader() { Close(); }

	CDataFileReader &operator=(CDataFileReader &&Other)
	{
		m_pDataFile = Other.m_pDataFile;
		Other.m_pDataFile = nullptr;
		return *this;
	}

	bool Open(class IStorage *pStorage, const char *pFilename, int StorageType);
	bool Close();
	bool IsOpen() const { return m_pDataFile != nullptr; }
	IOHANDLE File() const;

	int GetDataSize(int Index) const;
	void *GetData(int Index);
	void *GetDataSwapped(int Index); // makes sure that the data is 32bit LE ints when saved
	const char *GetDataString(int Index);
	void ReplaceData(int Index, char *pData, size_t Size); // memory for data must have been allocated with malloc
	void UnloadData(int Index);
	int NumData() const;

	int GetItemSize(int Index) const;
	void *GetItem(int Index, int *pType = nullptr, int *pID = nullptr);
	void GetType(int Type, int *pStart, int *pNum);
	int FindItemIndex(int Type, int ID);
	void *FindItem(int Type, int ID);
	int NumItems() const;

	SHA256_DIGEST Sha256() const;
	unsigned Crc() const;
	int MapSize() const;
};

// write access
class CDataFileWriter
{
	struct CDataInfo
	{
		void *m_pUncompressedData;
		int m_UncompressedSize;
		void *m_pCompressedData;
		int m_CompressedSize;
		int m_CompressionLevel;
	};

	struct CItemInfo
	{
		int m_Type;
		int m_ID;
		int m_Size;
		int m_Next;
		int m_Prev;
		void *m_pData;
	};

	struct CItemTypeInfo
	{
		int m_Num;
		int m_First;
		int m_Last;
	};

	enum
	{
		MAX_ITEM_TYPES = 0x10000,
	};

	IOHANDLE m_File;
	std::array<CItemTypeInfo, MAX_ITEM_TYPES> m_aItemTypes;
	std::vector<CItemInfo> m_vItems;
	std::vector<CDataInfo> m_vDatas;
	std::vector<int> m_vExtendedItemTypes;

	int GetTypeFromIndex(int Index) const;
	int GetExtendedItemTypeIndex(int Type);

public:
	CDataFileWriter();
	CDataFileWriter(CDataFileWriter &&Other)
	{
		m_File = Other.m_File;
		Other.m_File = 0;
		m_aItemTypes = std::move(Other.m_aItemTypes);
		m_vItems = std::move(Other.m_vItems);
		m_vDatas = std::move(Other.m_vDatas);
		m_vExtendedItemTypes = std::move(Other.m_vExtendedItemTypes);
	}
	~CDataFileWriter();

	bool Open(class IStorage *pStorage, const char *pFilename, int StorageType = IStorage::TYPE_SAVE);
	int AddItem(int Type, int ID, size_t Size, const void *pData);
	int AddData(size_t Size, const void *pData, int CompressionLevel = Z_DEFAULT_COMPRESSION);
	int AddDataSwapped(size_t Size, const void *pData);
	int AddDataString(const char *pStr);
	void Finish();
};

#endif
