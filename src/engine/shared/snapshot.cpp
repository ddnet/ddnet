/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "snapshot.h"

#include "compression.h"
#include "uuid_manager.h"

#include <base/bytes.h>
#include <base/dbg.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <generated/protocol7.h>
#include <generated/protocolglue.h>

#include <cstdlib>
#include <limits>

// CSnapshot

const CSnapshotItem *CSnapshot::GetItem(int Index) const
{
	return (const CSnapshotItem *)(DataStart() + Offsets()[Index]);
}

const CSnapshot CSnapshot::ms_EmptySnapshot;

int CSnapshot::GetItemSize(int Index) const
{
	if(Index == m_NumItems - 1)
		return (m_DataSize - Offsets()[Index]) - sizeof(CSnapshotItem);
	return (Offsets()[Index + 1] - Offsets()[Index]) - sizeof(CSnapshotItem);
}

int CSnapshot::GetItemType(int Index) const
{
	int InternalType = GetItem(Index)->InternalType();
	return GetExternalItemType(InternalType);
}

int CSnapshot::GetExternalItemType(int InternalType) const
{
	if(InternalType < OFFSET_UUID_TYPE)
	{
		return InternalType;
	}

	int TypeItemIndex = GetItemIndex(InternalType); // NETOBJTYPE_EX
	if(TypeItemIndex == -1 || GetItemSize(TypeItemIndex) < (int)sizeof(CUuid))
	{
		return -1;
	}
	const CSnapshotItem *pTypeItem = GetItem(TypeItemIndex);
	CUuid Uuid;
	for(size_t i = 0; i < sizeof(CUuid) / sizeof(int32_t); i++)
		uint_to_bytes_be(&Uuid.m_aData[i * sizeof(int32_t)], pTypeItem->Data()[i]);

	return g_UuidManager.LookupUuid(Uuid);
}

int CSnapshot::GetItemIndex(int Key) const
{
	// TODO: OPT: this should not be a linear search. very bad
	for(int i = 0; i < m_NumItems; i++)
	{
		if(GetItem(i)->Key() == Key)
			return i;
	}
	return -1;
}

void CSnapshot::InvalidateItem(int Index)
{
	((CSnapshotItem *)(DataStart() + Offsets()[Index]))->Invalidate();
}

const void *CSnapshot::FindItem(int Type, int Id) const
{
	int InternalType = Type;
	if(Type >= OFFSET_UUID)
	{
		CUuid TypeUuid = g_UuidManager.GetUuid(Type);
		int aTypeUuidItem[sizeof(CUuid) / sizeof(int32_t)];
		for(size_t i = 0; i < sizeof(CUuid) / sizeof(int32_t); i++)
			aTypeUuidItem[i] = bytes_be_to_uint(&TypeUuid.m_aData[i * sizeof(int32_t)]);

		bool Found = false;
		for(int i = 0; i < m_NumItems; i++)
		{
			const CSnapshotItem *pItem = GetItem(i);
			if(pItem->InternalType() == 0 && pItem->Id() >= OFFSET_UUID_TYPE) // NETOBJTYPE_EX
			{
				if(mem_comp(pItem->Data(), aTypeUuidItem, sizeof(CUuid)) == 0)
				{
					InternalType = pItem->Id();
					Found = true;
					break;
				}
			}
		}
		if(!Found)
		{
			return nullptr;
		}
	}
	int Index = GetItemIndex((InternalType << 16) | Id);
	return Index < 0 ? nullptr : GetItem(Index)->Data();
}

rust::Slice<const int32_t> CSnapshot::AsSlice() const
{
	return rust::Slice<const int32_t>((const int32_t *)this, TotalSize() / sizeof(int32_t));
}

unsigned CSnapshot::Crc() const
{
	unsigned int Crc = 0;

	for(int i = 0; i < m_NumItems; i++)
	{
		const CSnapshotItem *pItem = GetItem(i);
		int Size = GetItemSize(i);

		for(size_t b = 0; b < Size / sizeof(int32_t); b++)
			Crc += pItem->Data()[b];
	}
	return Crc;
}

void CSnapshot::DebugDump() const
{
	dbg_msg("snapshot", "data_size=%d num_items=%d", m_DataSize, m_NumItems);
	for(int i = 0; i < m_NumItems; i++)
	{
		const CSnapshotItem *pItem = GetItem(i);
		int Size = GetItemSize(i);
		dbg_msg("snapshot", "\ttype=%d id=%d", pItem->InternalType(), pItem->Id());
		for(size_t b = 0; b < Size / sizeof(int32_t); b++)
			dbg_msg("snapshot", "\t\t%3d %12d\t%08x", (int)b, pItem->Data()[b], pItem->Data()[b]);
	}
}

bool CSnapshot::IsValid(size_t ActualSize) const
{
	// validate total size
	if(ActualSize < sizeof(CSnapshot) ||
		ActualSize > MAX_SIZE ||
		m_NumItems < 0 ||
		m_NumItems > MAX_ITEMS ||
		m_DataSize < 0 ||
		ActualSize != TotalSize())
	{
		return false;
	}

	// validate item offsets
	const int *pOffsets = Offsets();
	for(int Index = 0; Index < m_NumItems; Index++)
		if(pOffsets[Index] < 0 || pOffsets[Index] > m_DataSize)
			return false;

	// validate item sizes
	for(int Index = 0; Index < m_NumItems; Index++)
		if(GetItemSize(Index) < 0) // the offsets must be validated before using this
			return false;

	return true;
}

// CSnapshotBuffer

std::unique_ptr<CSnapshotBuffer> CSnapshotBuffer_New()
{
	return std::make_unique<CSnapshotBuffer>();
}

// CSnapshotStorage

void CSnapshotStorage::Init()
{
	m_pFirst = nullptr;
	m_pLast = nullptr;
}

void CSnapshotStorage::PurgeAll()
{
	while(m_pFirst)
	{
		CHolder *pNext = m_pFirst->m_pNext;
		free(m_pFirst->m_pSnap);
		free(m_pFirst->m_pAltSnap);
		free(m_pFirst);
		m_pFirst = pNext;
	}
	m_pLast = nullptr;
}

void CSnapshotStorage::PurgeUntil(int Tick)
{
	CHolder *pHolder = m_pFirst;

	while(pHolder)
	{
		CHolder *pNext = pHolder->m_pNext;
		if(pHolder->m_Tick >= Tick)
			return; // no more to remove
		free(pHolder->m_pSnap);
		free(pHolder->m_pAltSnap);
		free(pHolder);

		// did we come to the end of the list?
		if(!pNext)
			break;

		m_pFirst = pNext;
		pNext->m_pPrev = nullptr;
		pHolder = pNext;
	}

	// no more snapshots in storage
	m_pFirst = nullptr;
	m_pLast = nullptr;
}

void CSnapshotStorage::Add(int Tick, int64_t Tagtime, size_t DataSize, const void *pData, size_t AltDataSize, const void *pAltData)
{
	dbg_assert(DataSize <= (size_t)CSnapshot::MAX_SIZE, "Snapshot data size invalid");
	dbg_assert(AltDataSize <= (size_t)CSnapshot::MAX_SIZE, "Alt snapshot data size invalid");

	CHolder *pHolder = static_cast<CHolder *>(malloc(sizeof(CHolder)));
	pHolder->m_Tick = Tick;
	pHolder->m_Tagtime = Tagtime;

	pHolder->m_pSnap = static_cast<CSnapshot *>(malloc(DataSize));
	mem_copy(pHolder->m_pSnap, pData, DataSize);
	pHolder->m_SnapSize = DataSize;

	if(AltDataSize) // create alternative if wanted
	{
		pHolder->m_pAltSnap = static_cast<CSnapshot *>(malloc(AltDataSize));
		mem_copy(pHolder->m_pAltSnap, pAltData, AltDataSize);
		pHolder->m_AltSnapSize = AltDataSize;
	}
	else
	{
		pHolder->m_pAltSnap = nullptr;
		pHolder->m_AltSnapSize = 0;
	}

	// link
	pHolder->m_pNext = nullptr;
	pHolder->m_pPrev = m_pLast;
	if(m_pLast)
		m_pLast->m_pNext = pHolder;
	else
		m_pFirst = pHolder;
	m_pLast = pHolder;
}

int CSnapshotStorage::Get(int Tick, int64_t *pTagtime, const CSnapshot **ppData, const CSnapshot **ppAltData) const
{
	CHolder *pHolder = m_pFirst;

	while(pHolder)
	{
		if(pHolder->m_Tick == Tick)
		{
			if(pTagtime)
				*pTagtime = pHolder->m_Tagtime;
			if(ppData)
				*ppData = pHolder->m_pSnap;
			if(ppAltData)
				*ppAltData = pHolder->m_pAltSnap;
			return pHolder->m_SnapSize;
		}

		pHolder = pHolder->m_pNext;
	}

	return -1;
}
