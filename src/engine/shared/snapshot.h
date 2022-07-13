/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_SNAPSHOT_H
#define ENGINE_SHARED_SNAPSHOT_H

#include <stdint.h>

// CSnapshot

class CSnapshotItem
{
public:
	int m_TypeAndID = 0;

	int *Data() { return (int *)(this + 1); }
	int Type() const { return m_TypeAndID >> 16; }
	int ID() const { return m_TypeAndID & 0xffff; }
	int Key() const { return m_TypeAndID; }
};

class CSnapshot
{
	friend class CSnapshotBuilder;
	int m_DataSize = 0;
	int m_NumItems = 0;

	int *Offsets() const { return (int *)(this + 1); }
	char *DataStart() const { return (char *)(Offsets() + m_NumItems); }

public:
	enum
	{
		OFFSET_UUID_TYPE = 0x4000,
		MAX_TYPE = 0x7fff,
		MAX_ID = 0xffff,
		MAX_ITEMS = 1024,
		MAX_PARTS = 64,
		MAX_SIZE = MAX_PARTS * 1024
	};

	void Clear()
	{
		m_DataSize = 0;
		m_NumItems = 0;
	}
	int NumItems() const { return m_NumItems; }
	CSnapshotItem *GetItem(int Index) const;
	int GetItemSize(int Index) const;
	int GetItemIndex(int Key) const;
	int GetItemType(int Index) const;
	int GetExternalItemType(int InternalType) const;
	void *FindItem(int Type, int ID) const;

	unsigned Crc();
	void DebugDump();
};

// CSnapshotDelta

class CSnapshotDelta
{
public:
	class CData
	{
	public:
		int m_NumDeletedItems = 0;
		int m_NumUpdateItems = 0;
		int m_NumTempItems = 0; // needed?
		int m_aData[1] = {0};
	};

private:
	enum
	{
		MAX_NETOBJSIZES = 64
	};
	short m_aItemSizes[MAX_NETOBJSIZES] = {0};
	int m_aSnapshotDataRate[CSnapshot::MAX_TYPE + 1] = {0};
	int m_aSnapshotDataUpdates[CSnapshot::MAX_TYPE + 1] = {0};
	CData m_Empty;

	static void UndiffItem(int *pPast, int *pDiff, int *pOut, int Size, int *pDataRate);

public:
	static int DiffItem(int *pPast, int *pCurrent, int *pOut, int Size);
	CSnapshotDelta();
	CSnapshotDelta(const CSnapshotDelta &Old);
	int GetDataRate(int Index) const { return m_aSnapshotDataRate[Index]; }
	int GetDataUpdates(int Index) const { return m_aSnapshotDataUpdates[Index]; }
	void SetStaticsize(int ItemType, int Size);
	const CData *EmptyDelta() const;
	int CreateDelta(class CSnapshot *pFrom, class CSnapshot *pTo, void *pDstData);
	int UnpackDelta(class CSnapshot *pFrom, class CSnapshot *pTo, const void *pSrcData, int DataSize);
};

// CSnapshotStorage

class CSnapshotStorage
{
public:
	class CHolder
	{
	public:
		CHolder *m_pPrev = nullptr;
		CHolder *m_pNext = nullptr;

		int64_t m_Tagtime = 0;
		int m_Tick = 0;

		int m_SnapSize = 0;
		int m_AltSnapSize = 0;

		CSnapshot *m_pSnap = nullptr;
		CSnapshot *m_pAltSnap = nullptr;
	};

	CHolder *m_pFirst = nullptr;
	CHolder *m_pLast = nullptr;

	CSnapshotStorage() { Init(); }
	~CSnapshotStorage() { PurgeAll(); }
	void Init();
	void PurgeAll();
	void PurgeUntil(int Tick);
	void Add(int Tick, int64_t Tagtime, int DataSize, void *pData, int AltDataSize, void *pAltData);
	int Get(int Tick, int64_t *pTagtime, CSnapshot **ppData, CSnapshot **ppAltData);
};

class CSnapshotBuilder
{
	enum
	{
		MAX_EXTENDED_ITEM_TYPES = 64,
	};

	char m_aData[CSnapshot::MAX_SIZE] = {0};
	int m_DataSize = 0;

	int m_aOffsets[CSnapshot::MAX_ITEMS] = {0};
	int m_NumItems = 0;

	int m_aExtendedItemTypes[MAX_EXTENDED_ITEM_TYPES] = {0};
	int m_NumExtendedItemTypes = 0;

	void AddExtendedItemType(int Index);
	int GetExtendedItemTypeIndex(int TypeID);
	int GetTypeFromIndex(int Index);

	bool m_Sixup = false;

public:
	CSnapshotBuilder();

	void Init(bool Sixup = false);

	void *NewItem(int Type, int ID, int Size);

	CSnapshotItem *GetItem(int Index);
	int *GetItemData(int Key);

	int Finish(void *pSnapdata);
};

#endif // ENGINE_SNAPSHOT_H
