/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_SNAPSHOT_H
#define ENGINE_SHARED_SNAPSHOT_H

#include <cstddef>
#include <cstdint>

#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

// CSnapshot

class CSnapshotItem
{
	friend class CSnapshotBuilder;

	int *Data() { return (int *)(this + 1); }

public:
	int m_TypeAndId;

	const int *Data() const { return (int *)(this + 1); }
	int Type() const { return m_TypeAndId >> 16; }
	int Id() const { return m_TypeAndId & 0xffff; }
	int Key() const { return m_TypeAndId; }
	void Invalidate() { m_TypeAndId = -1; }
};

class CSnapshot
{
	friend class CSnapshotBuilder;
	int m_DataSize = 0;
	int m_NumItems = 0;

	int *Offsets() const { return (int *)(this + 1); }
	char *DataStart() const { return (char *)(Offsets() + m_NumItems); }

	size_t OffsetSize() const { return sizeof(int) * m_NumItems; }
	size_t TotalSize() const { return sizeof(CSnapshot) + OffsetSize() + m_DataSize; }

	static const CSnapshot ms_EmptySnapshot;

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

	int NumItems() const { return m_NumItems; }
	const CSnapshotItem *GetItem(int Index) const;
	int GetItemSize(int Index) const;
	int GetItemIndex(int Key) const;
	void InvalidateItem(int Index);
	int GetItemType(int Index) const;
	int GetExternalItemType(int InternalType) const;
	const void *FindItem(int Type, int Id) const;

	unsigned Crc() const;
	void DebugDump() const;
	bool IsValid(size_t ActualSize) const;

	static const CSnapshot *EmptySnapshot() { return &ms_EmptySnapshot; }
};

// CSnapshotDelta

class CSnapshotDelta
{
public:
	class CData
	{
	public:
		int m_NumDeletedItems;
		int m_NumUpdateItems;
		int m_NumTempItems; // needed?
		int m_aData[1];
	};

private:
	enum
	{
		MAX_NETOBJSIZES = 64
	};
	short m_aItemSizes[MAX_NETOBJSIZES];
	short m_aItemSizes7[MAX_NETOBJSIZES];
	uint64_t m_aSnapshotDataRate[CSnapshot::MAX_TYPE + 1];
	uint64_t m_aSnapshotDataUpdates[CSnapshot::MAX_TYPE + 1];
	CData m_Empty;

	static void UndiffItem(const int *pPast, const int *pDiff, int *pOut, int Size, uint64_t *pDataRate);

public:
	static int DiffItem(const int *pPast, const int *pCurrent, int *pOut, int Size);
	CSnapshotDelta();
	CSnapshotDelta(const CSnapshotDelta &Old);
	uint64_t GetDataRate(int Index) const { return m_aSnapshotDataRate[Index]; }
	uint64_t GetDataUpdates(int Index) const { return m_aSnapshotDataUpdates[Index]; }
	void SetStaticsize(int ItemType, size_t Size);
	void SetStaticsize7(int ItemType, size_t Size);
	const CData *EmptyDelta() const;
	int CreateDelta(const CSnapshot *pFrom, const CSnapshot *pTo, void *pDstData);
	int UnpackDelta(const CSnapshot *pFrom, CSnapshot *pTo, const void *pSrcData, int DataSize, bool Sixup);
	int DebugDumpDelta(const void *pSrcData, int DataSize);
};

// CSnapshotStorage

class CSnapshotStorage
{
public:
	class CHolder
	{
	public:
		CHolder *m_pPrev;
		CHolder *m_pNext;

		int64_t m_Tagtime;
		int m_Tick;

		int m_SnapSize;
		int m_AltSnapSize;

		CSnapshot *m_pSnap;
		CSnapshot *m_pAltSnap;
	};

	CHolder *m_pFirst;
	CHolder *m_pLast;

	CSnapshotStorage() { Init(); }
	~CSnapshotStorage() { PurgeAll(); }
	void Init();
	void PurgeAll();
	void PurgeUntil(int Tick);
	void Add(int Tick, int64_t Tagtime, size_t DataSize, const void *pData, size_t AltDataSize, const void *pAltData);
	int Get(int Tick, int64_t *pTagtime, const CSnapshot **ppData, const CSnapshot **ppAltData) const;
};

class CSnapshotBuilder
{
	enum
	{
		MAX_EXTENDED_ITEM_TYPES = 64,
	};

	char m_aData[CSnapshot::MAX_SIZE];
	int m_DataSize;

	int m_aOffsets[CSnapshot::MAX_ITEMS];
	int m_NumItems;

	int m_aExtendedItemTypes[MAX_EXTENDED_ITEM_TYPES];
	int m_NumExtendedItemTypes;

	bool AddExtendedItemType(int Index);
	int GetExtendedItemTypeIndex(int TypeId);
	int GetTypeFromIndex(int Index) const;

	bool m_Sixup = false;

public:
	CSnapshotBuilder();

	void Init(bool Sixup = false);
	void Init7(const CSnapshot *pSnapshot);

	void *NewItem(int Type, int Id, int Size);

	CSnapshotItem *GetItem(int Index);
	int *GetItemData(int Key);

	int Finish(void *pSnapdata);
};

#endif // ENGINE_SNAPSHOT_H
