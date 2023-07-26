/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_DATAFILE_H
#define ENGINE_SHARED_DATAFILE_H

#include <engine/storage.h>

#include <base/hash.h>
#include <base/system.h>

#include <zlib.h>

enum
{
	ITEMTYPE_EX = 0xffff,
};

// raw datafile access
class CDataFileReader
{
	struct CDatafile *m_pDataFile;
	void *GetDataImpl(int Index, int Swap);
	int GetFileDataSize(int Index) const;

	int GetExternalItemType(int InternalType);
	int GetInternalItemType(int ExternalType);

public:
	CDataFileReader() :
		m_pDataFile(nullptr) {}
	~CDataFileReader() { Close(); }

	bool Open(class IStorage *pStorage, const char *pFilename, int StorageType);
	bool Close();
	bool IsOpen() const { return m_pDataFile != nullptr; }
	IOHANDLE File() const;

	void *GetData(int Index);
	void *GetDataSwapped(int Index); // makes sure that the data is 32bit LE ints when saved
	int GetDataSize(int Index) const;
	void ReplaceData(int Index, char *pData, size_t Size); // memory for data must have been allocated with malloc
	void UnloadData(int Index);
	int NumData() const;

	void *GetItem(int Index, int *pType = nullptr, int *pID = nullptr);
	int GetItemSize(int Index) const;
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
		MAX_ITEMS = 1024,
		MAX_DATAS = 1024,
		MAX_EXTENDED_ITEM_TYPES = 64,
	};

	IOHANDLE m_File;
	int m_NumItems;
	int m_NumDatas;
	int m_NumItemTypes;
	int m_NumExtendedItemTypes;
	CItemTypeInfo *m_pItemTypes;
	CItemInfo *m_pItems;
	CDataInfo *m_pDatas;
	int m_aExtendedItemTypes[MAX_EXTENDED_ITEM_TYPES];

	int GetTypeFromIndex(int Index) const;
	int GetExtendedItemTypeIndex(int Type);

public:
	CDataFileWriter();
	CDataFileWriter(CDataFileWriter &&Other) :
		m_NumItems(Other.m_NumItems),
		m_NumDatas(Other.m_NumDatas),
		m_NumItemTypes(Other.m_NumItemTypes),
		m_NumExtendedItemTypes(Other.m_NumExtendedItemTypes)
	{
		m_File = Other.m_File;
		Other.m_File = 0;
		m_pItemTypes = Other.m_pItemTypes;
		Other.m_pItemTypes = nullptr;
		m_pItems = Other.m_pItems;
		Other.m_pItems = nullptr;
		m_pDatas = Other.m_pDatas;
		Other.m_pDatas = nullptr;
		mem_copy(m_aExtendedItemTypes, Other.m_aExtendedItemTypes, sizeof(m_aExtendedItemTypes));
	}
	~CDataFileWriter();

	void Init();
	bool OpenFile(class IStorage *pStorage, const char *pFilename, int StorageType = IStorage::TYPE_SAVE);
	bool Open(class IStorage *pStorage, const char *pFilename, int StorageType = IStorage::TYPE_SAVE);
	int AddData(int Size, const void *pData, int CompressionLevel = Z_DEFAULT_COMPRESSION);
	int AddDataSwapped(int Size, const void *pData);
	int AddItem(int Type, int ID, int Size, const void *pData);
	void Finish();
};

#endif
