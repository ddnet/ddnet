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
	struct CDatafile *m_pDataFile = nullptr;
	void *GetDataImpl(int Index, int Swap);
	int GetFileDataSize(int Index);

	int GetExternalItemType(int InternalType);
	int GetInternalItemType(int ExternalType);

public:
	CDataFileReader() :
		m_pDataFile(nullptr) {}
	~CDataFileReader() { Close(); }

	bool IsOpen() const { return m_pDataFile != nullptr; }

	bool Open(class IStorage *pStorage, const char *pFilename, int StorageType);
	bool Close();

	void *GetData(int Index);
	void *GetDataSwapped(int Index); // makes sure that the data is 32bit LE ints when saved
	int GetDataSize(int Index);
	void UnloadData(int Index);
	void *GetItem(int Index, int *pType, int *pID);
	int GetItemSize(int Index) const;
	void GetType(int Type, int *pStart, int *pNum);
	int FindItemIndex(int Type, int ID);
	void *FindItem(int Type, int ID);
	int NumItems() const;
	int NumData() const;
	void Unload();

	SHA256_DIGEST Sha256() const;
	unsigned Crc() const;
	int MapSize() const;
	IOHANDLE File();
};

// write access
class CDataFileWriter
{
	struct CDataInfo
	{
		int m_UncompressedSize = 0;
		int m_CompressedSize = 0;
		void *m_pCompressedData = nullptr;
	};

	struct CItemInfo
	{
		int m_Type = 0;
		int m_ID = 0;
		int m_Size = 0;
		int m_Next = 0;
		int m_Prev = 0;
		void *m_pData = nullptr;
	};

	struct CItemTypeInfo
	{
		int m_Num = 0;
		int m_First = 0;
		int m_Last = 0;
	};

	enum
	{
		MAX_ITEM_TYPES = 0x10000,
		MAX_ITEMS = 1024,
		MAX_DATAS = 1024,
		MAX_EXTENDED_ITEM_TYPES = 64,
	};

	IOHANDLE m_File;
	int m_NumItems = 0;
	int m_NumDatas = 0;
	int m_NumItemTypes = 0;
	int m_NumExtendedItemTypes = 0;
	CItemTypeInfo *m_pItemTypes = nullptr;
	CItemInfo *m_pItems = nullptr;
	CDataInfo *m_pDatas = nullptr;
	int m_aExtendedItemTypes[MAX_EXTENDED_ITEM_TYPES] = {0};

	int GetExtendedItemTypeIndex(int Type);
	int GetTypeFromIndex(int Index);

public:
	CDataFileWriter();
	~CDataFileWriter();
	void Init();
	bool OpenFile(class IStorage *pStorage, const char *pFilename, int StorageType = IStorage::TYPE_SAVE);
	bool Open(class IStorage *pStorage, const char *pFilename, int StorageType = IStorage::TYPE_SAVE);
	int AddData(int Size, void *pData, int CompressionLevel = Z_DEFAULT_COMPRESSION);
	int AddDataSwapped(int Size, void *pData);
	int AddItem(int Type, int ID, int Size, void *pData);
	int Finish();
};

#endif
