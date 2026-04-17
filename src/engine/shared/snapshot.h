/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_SNAPSHOT_H
#define ENGINE_SHARED_SNAPSHOT_H

#include <base/cxx.h>

#include <cstddef>
#include <cstdint>
#include <memory>

// CSnapshot

class CSnapshotItem
{
	int *Data() { return (int *)(this + 1); }

public:
	int m_TypeAndId;

	const int *Data() const { return (int *)(this + 1); }
	int InternalType() const { return m_TypeAndId >> 16; }
	int Id() const { return m_TypeAndId & 0xffff; }
	int Key() const { return m_TypeAndId; }
	void Invalidate() { m_TypeAndId = -1; }
};

class CSnapshot
{
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
	int DataSize() const { return m_DataSize; }
	const CSnapshotItem *GetItem(int Index) const;
	int GetItemSize(int Index) const;
	int GetItemIndex(int Key) const;
	void InvalidateItem(int Index);
	int GetItemType(int Index) const;
	int GetExternalItemType(int InternalType) const;
	const void *FindItem(int Type, int Id) const;

	rust::Slice<const int32_t> AsSlice() const;

	unsigned Crc() const;
	// Prints the raw snapshot data showing item and int boundaries.
	// See also `CNetObjHandler::DebugDumpSnapshot(const CSnapshot *pSnap)`
	// For more detailed annotations of the data.
	void DebugDump() const;
	bool IsValid(size_t ActualSize) const;

	static const CSnapshot *EmptySnapshot() { return &ms_EmptySnapshot; }
};

class alignas(int32_t) CSnapshotBuffer
{
public:
	unsigned char m_aData[CSnapshot::MAX_SIZE];

	CSnapshot *AsSnapshot() { return (CSnapshot *)m_aData; }
	const CSnapshot *AsSnapshot() const { return (const CSnapshot *)m_aData; }
	rust::Slice<int32_t> AsMutSlice() { return rust::Slice((int32_t *)m_aData, sizeof(m_aData) / sizeof(int32_t)); }
};

std::unique_ptr<CSnapshotBuffer> CSnapshotBuffer_New();

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

#include <engine/shared/snapshot/builder.h> // NOLINT(misc-header-include-cycle)
#include <engine/shared/snapshot/delta.h> // NOLINT(misc-header-include-cycle)

#endif // ENGINE_SHARED_SNAPSHOT_H
