/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "snapshot.h"
#include "compression.h"
#include "uuid_manager.h"

#include <climits>
#include <cstdlib>

#include <base/system.h>
#include <game/generated/protocolglue.h>

// CSnapshot

CSnapshotItem *CSnapshot::GetItem(int Index) const
{
	return (CSnapshotItem *)(DataStart() + Offsets()[Index]);
}

int CSnapshot::GetItemSize(int Index) const
{
	if(Index == m_NumItems - 1)
		return (m_DataSize - Offsets()[Index]) - sizeof(CSnapshotItem);
	return (Offsets()[Index + 1] - Offsets()[Index]) - sizeof(CSnapshotItem);
}

int CSnapshot::GetItemType(int Index) const
{
	int InternalType = GetItem(Index)->Type();
	return GetExternalItemType(InternalType);
}

int CSnapshot::GetExternalItemType(int InternalType) const
{
	if(InternalType < OFFSET_UUID_TYPE)
	{
		return InternalType;
	}

	int TypeItemIndex = GetItemIndex((0 << 16) | InternalType); // NETOBJTYPE_EX
	if(TypeItemIndex == -1 || GetItemSize(TypeItemIndex) < (int)sizeof(CUuid))
	{
		return InternalType;
	}
	CSnapshotItem *pTypeItem = GetItem(TypeItemIndex);
	CUuid Uuid;
	for(int i = 0; i < (int)sizeof(CUuid) / 4; i++)
		int_to_bytes_be(&Uuid.m_aData[i * 4], pTypeItem->Data()[i]);

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

void *CSnapshot::FindItem(int Type, int ID) const
{
	int InternalType = Type;
	if(Type >= OFFSET_UUID)
	{
		CUuid TypeUuid = g_UuidManager.GetUuid(Type);
		int aTypeUuidItem[sizeof(CUuid) / 4];
		for(int i = 0; i < (int)sizeof(CUuid) / 4; i++)
			aTypeUuidItem[i] = bytes_be_to_int(&TypeUuid.m_aData[i * 4]);

		bool Found = false;
		for(int i = 0; i < m_NumItems; i++)
		{
			CSnapshotItem *pItem = GetItem(i);
			if(pItem->Type() == 0 && pItem->ID() >= OFFSET_UUID_TYPE) // NETOBJTYPE_EX
			{
				if(mem_comp(pItem->Data(), aTypeUuidItem, sizeof(CUuid)) == 0)
				{
					InternalType = pItem->ID();
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
	int Index = GetItemIndex((InternalType << 16) | ID);
	return Index < 0 ? nullptr : GetItem(Index)->Data();
}

unsigned CSnapshot::Crc()
{
	unsigned int Crc = 0;

	for(int i = 0; i < m_NumItems; i++)
	{
		CSnapshotItem *pItem = GetItem(i);
		int Size = GetItemSize(i);

		for(int b = 0; b < Size / 4; b++)
			Crc += pItem->Data()[b];
	}
	return Crc;
}

void CSnapshot::DebugDump()
{
	dbg_msg("snapshot", "data_size=%d num_items=%d", m_DataSize, m_NumItems);
	for(int i = 0; i < m_NumItems; i++)
	{
		CSnapshotItem *pItem = GetItem(i);
		int Size = GetItemSize(i);
		dbg_msg("snapshot", "\ttype=%d id=%d", pItem->Type(), pItem->ID());
		for(int b = 0; b < Size / 4; b++)
			dbg_msg("snapshot", "\t\t%3d %12d\t%08x", b, pItem->Data()[b], pItem->Data()[b]);
	}
}

bool CSnapshot::IsValid(size_t ActualSize) const
{
	// validate total size
	if(ActualSize < sizeof(CSnapshot) || m_NumItems < 0 || m_DataSize < 0 || ActualSize != TotalSize())
		return false;

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

// CSnapshotDelta

enum
{
	HASHLIST_SIZE = 256,
	HASHLIST_BUCKET_SIZE = 64,
};

struct CItemList
{
	int m_Num;
	int m_aKeys[HASHLIST_BUCKET_SIZE];
	int m_aIndex[HASHLIST_BUCKET_SIZE];
};

inline size_t CalcHashID(int Key)
{
	// djb2 (http://www.cse.yorku.ca/~oz/hash.html)
	unsigned Hash = 5381;
	for(unsigned Shift = 0; Shift < sizeof(int); Shift++)
		Hash = ((Hash << 5) + Hash) + ((Key >> (Shift * 8)) & 0xFF);
	return Hash % HASHLIST_SIZE;
}

static void GenerateHash(CItemList *pHashlist, CSnapshot *pSnapshot)
{
	for(int i = 0; i < HASHLIST_SIZE; i++)
		pHashlist[i].m_Num = 0;

	for(int i = 0; i < pSnapshot->NumItems(); i++)
	{
		int Key = pSnapshot->GetItem(i)->Key();
		size_t HashID = CalcHashID(Key);
		if(pHashlist[HashID].m_Num < HASHLIST_BUCKET_SIZE)
		{
			pHashlist[HashID].m_aIndex[pHashlist[HashID].m_Num] = i;
			pHashlist[HashID].m_aKeys[pHashlist[HashID].m_Num] = Key;
			pHashlist[HashID].m_Num++;
		}
	}
}

static int GetItemIndexHashed(int Key, const CItemList *pHashlist)
{
	size_t HashID = CalcHashID(Key);
	for(int i = 0; i < pHashlist[HashID].m_Num; i++)
	{
		if(pHashlist[HashID].m_aKeys[i] == Key)
			return pHashlist[HashID].m_aIndex[i];
	}

	return -1;
}

int CSnapshotDelta::DiffItem(int *pPast, int *pCurrent, int *pOut, int Size)
{
	int Needed = 0;
	while(Size)
	{
		*pOut = *pCurrent - *pPast;
		Needed |= *pOut;
		pOut++;
		pPast++;
		pCurrent++;
		Size--;
	}

	return Needed;
}

void CSnapshotDelta::UndiffItem(int *pPast, int *pDiff, int *pOut, int Size, int *pDataRate)
{
	while(Size)
	{
		*pOut = *pPast + *pDiff;

		if(*pDiff == 0)
			*pDataRate += 1;
		else
		{
			unsigned char aBuf[CVariableInt::MAX_BYTES_PACKED];
			unsigned char *pEnd = CVariableInt::Pack(aBuf, *pDiff, sizeof(aBuf));
			*pDataRate += (int)(pEnd - (unsigned char *)aBuf) * 8;
		}

		pOut++;
		pPast++;
		pDiff++;
		Size--;
	}
}

CSnapshotDelta::CSnapshotDelta()
{
	mem_zero(m_aItemSizes, sizeof(m_aItemSizes));
	mem_zero(m_aSnapshotDataRate, sizeof(m_aSnapshotDataRate));
	mem_zero(m_aSnapshotDataUpdates, sizeof(m_aSnapshotDataUpdates));
	mem_zero(&m_Empty, sizeof(m_Empty));
}

CSnapshotDelta::CSnapshotDelta(const CSnapshotDelta &Old)
{
	mem_copy(m_aItemSizes, Old.m_aItemSizes, sizeof(m_aItemSizes));
	mem_copy(m_aSnapshotDataRate, Old.m_aSnapshotDataRate, sizeof(m_aSnapshotDataRate));
	mem_copy(m_aSnapshotDataUpdates, Old.m_aSnapshotDataUpdates, sizeof(m_aSnapshotDataUpdates));
	mem_zero(&m_Empty, sizeof(m_Empty));
}

void CSnapshotDelta::SetStaticsize(int ItemType, int Size)
{
	if(ItemType < 0 || ItemType >= MAX_NETOBJSIZES)
		return;
	m_aItemSizes[ItemType] = Size;
}

const CSnapshotDelta::CData *CSnapshotDelta::EmptyDelta() const
{
	return &m_Empty;
}

// TODO: OPT: this should be made much faster
int CSnapshotDelta::CreateDelta(CSnapshot *pFrom, CSnapshot *pTo, void *pDstData)
{
	CData *pDelta = (CData *)pDstData;
	int *pData = (int *)pDelta->m_aData;

	pDelta->m_NumDeletedItems = 0;
	pDelta->m_NumUpdateItems = 0;
	pDelta->m_NumTempItems = 0;

	CItemList aHashlist[HASHLIST_SIZE];
	GenerateHash(aHashlist, pTo);

	// pack deleted stuff
	for(int i = 0; i < pFrom->NumItems(); i++)
	{
		const CSnapshotItem *pFromItem = pFrom->GetItem(i);
		if(GetItemIndexHashed(pFromItem->Key(), aHashlist) == -1)
		{
			// deleted
			pDelta->m_NumDeletedItems++;
			*pData = pFromItem->Key();
			pData++;
		}
	}

	GenerateHash(aHashlist, pFrom);

	// fetch previous indices
	// we do this as a separate pass because it helps the cache
	int aPastIndices[CSnapshot::MAX_ITEMS];
	const int NumItems = pTo->NumItems();
	for(int i = 0; i < NumItems; i++)
	{
		const CSnapshotItem *pCurItem = pTo->GetItem(i); // O(1) .. O(n)
		aPastIndices[i] = GetItemIndexHashed(pCurItem->Key(), aHashlist); // O(n) .. O(n^n)
	}

	for(int i = 0; i < NumItems; i++)
	{
		// do delta
		const int ItemSize = pTo->GetItemSize(i); // O(1) .. O(n)
		CSnapshotItem *pCurItem = pTo->GetItem(i); // O(1) .. O(n)
		const int PastIndex = aPastIndices[i];
		const bool IncludeSize = pCurItem->Type() >= MAX_NETOBJSIZES || !m_aItemSizes[pCurItem->Type()];

		if(PastIndex != -1)
		{
			int *pItemDataDst = pData + 3;

			CSnapshotItem *pPastItem = pFrom->GetItem(PastIndex);

			if(!IncludeSize)
				pItemDataDst = pData + 2;

			if(DiffItem(pPastItem->Data(), pCurItem->Data(), pItemDataDst, ItemSize / 4))
			{
				*pData++ = pCurItem->Type();
				*pData++ = pCurItem->ID();
				if(IncludeSize)
					*pData++ = ItemSize / 4;
				pData += ItemSize / 4;
				pDelta->m_NumUpdateItems++;
			}
		}
		else
		{
			*pData++ = pCurItem->Type();
			*pData++ = pCurItem->ID();
			if(IncludeSize)
				*pData++ = ItemSize / 4;

			mem_copy(pData, pCurItem->Data(), ItemSize);
			pData += ItemSize / 4;
			pDelta->m_NumUpdateItems++;
		}
	}

	if(!pDelta->m_NumDeletedItems && !pDelta->m_NumUpdateItems && !pDelta->m_NumTempItems)
		return 0;

	return (int)((char *)pData - (char *)pDstData);
}

static int RangeCheck(void *pEnd, void *pPtr, int Size)
{
	if((const char *)pPtr + Size > (const char *)pEnd)
		return -1;
	return 0;
}

int CSnapshotDelta::UnpackDelta(CSnapshot *pFrom, CSnapshot *pTo, const void *pSrcData, int DataSize)
{
	CData *pDelta = (CData *)pSrcData;
	int *pData = (int *)pDelta->m_aData;
	int *pEnd = (int *)(((char *)pSrcData + DataSize));

	CSnapshotBuilder Builder;
	Builder.Init();

	// unpack deleted stuff
	int *pDeleted = pData;
	if(pDelta->m_NumDeletedItems < 0)
		return -1;
	pData += pDelta->m_NumDeletedItems;
	if(pData > pEnd)
		return -1;

	// copy all non deleted stuff
	for(int i = 0; i < pFrom->NumItems(); i++)
	{
		CSnapshotItem *pFromItem = pFrom->GetItem(i);
		const int ItemSize = pFrom->GetItemSize(i);
		bool Keep = true;
		for(int d = 0; d < pDelta->m_NumDeletedItems; d++)
		{
			if(pDeleted[d] == pFromItem->Key())
			{
				Keep = false;
				break;
			}
		}

		if(Keep)
		{
			void *pObj = Builder.NewItem(pFromItem->Type(), pFromItem->ID(), ItemSize);
			if(!pObj)
				return -4;

			// keep it
			mem_copy(pObj, pFromItem->Data(), ItemSize);
		}
	}

	// unpack updated stuff
	for(int i = 0; i < pDelta->m_NumUpdateItems; i++)
	{
		if(pData + 2 > pEnd)
			return -1;

		const int Type = *pData++;
		if(Type < 0 || Type > CSnapshot::MAX_TYPE)
			return -3;

		const int ID = *pData++;
		if(ID < 0 || ID > CSnapshot::MAX_ID)
			return -3;

		int ItemSize;
		if(Type < MAX_NETOBJSIZES && m_aItemSizes[Type])
			ItemSize = m_aItemSizes[Type];
		else
		{
			if(pData + 1 > pEnd)
				return -2;
			if(*pData < 0 || *pData > INT_MAX / 4)
				return -3;
			ItemSize = (*pData++) * 4;
		}

		if(ItemSize < 0 || RangeCheck(pEnd, pData, ItemSize))
			return -3;

		const int Key = (Type << 16) | ID;

		// create the item if needed
		int *pNewData = Builder.GetItemData(Key);
		if(!pNewData)
			pNewData = (int *)Builder.NewItem(Type, ID, ItemSize);

		if(!pNewData)
			return -4;

		const int FromIndex = pFrom->GetItemIndex(Key);
		if(FromIndex != -1)
		{
			// we got an update so we need to apply the diff
			UndiffItem(pFrom->GetItem(FromIndex)->Data(), pData, pNewData, ItemSize / 4, &m_aSnapshotDataRate[Type]);
		}
		else // no previous, just copy the pData
		{
			mem_copy(pNewData, pData, ItemSize);
			m_aSnapshotDataRate[Type] += ItemSize * 8;
		}
		m_aSnapshotDataUpdates[Type]++;

		pData += ItemSize / 4;
	}

	// finish up
	return Builder.Finish(pTo);
}

// CSnapshotStorage

void CSnapshotStorage::Init()
{
	m_pFirst = 0;
	m_pLast = 0;
}

void CSnapshotStorage::PurgeAll()
{
	CHolder *pHolder = m_pFirst;

	while(pHolder)
	{
		CHolder *pNext = pHolder->m_pNext;
		free(pHolder);
		pHolder = pNext;
	}

	// no more snapshots in storage
	m_pFirst = 0;
	m_pLast = 0;
}

void CSnapshotStorage::PurgeUntil(int Tick)
{
	CHolder *pHolder = m_pFirst;

	while(pHolder)
	{
		CHolder *pNext = pHolder->m_pNext;
		if(pHolder->m_Tick >= Tick)
			return; // no more to remove
		free(pHolder);

		// did we come to the end of the list?
		if(!pNext)
			break;

		m_pFirst = pNext;
		pNext->m_pPrev = 0x0;

		pHolder = pNext;
	}

	// no more snapshots in storage
	m_pFirst = 0;
	m_pLast = 0;
}

void CSnapshotStorage::Add(int Tick, int64_t Tagtime, int DataSize, void *pData, int AltDataSize, void *pAltData)
{
	// allocate memory for holder + snapshot_data
	int TotalSize = sizeof(CHolder) + DataSize;

	if(AltDataSize > 0)
	{
		TotalSize += AltDataSize;
	}

	CHolder *pHolder = (CHolder *)malloc(TotalSize);

	// set data
	pHolder->m_Tick = Tick;
	pHolder->m_Tagtime = Tagtime;
	pHolder->m_SnapSize = DataSize;
	pHolder->m_pSnap = (CSnapshot *)(pHolder + 1);
	mem_copy(pHolder->m_pSnap, pData, DataSize);

	if(AltDataSize > 0) // create alternative if wanted
	{
		pHolder->m_pAltSnap = (CSnapshot *)(((char *)pHolder->m_pSnap) + DataSize);
		mem_copy(pHolder->m_pAltSnap, pAltData, AltDataSize);
		pHolder->m_AltSnapSize = AltDataSize;
	}
	else
	{
		pHolder->m_pAltSnap = 0;
		pHolder->m_AltSnapSize = 0;
	}

	// link
	pHolder->m_pNext = 0;
	pHolder->m_pPrev = m_pLast;
	if(m_pLast)
		m_pLast->m_pNext = pHolder;
	else
		m_pFirst = pHolder;
	m_pLast = pHolder;
}

int CSnapshotStorage::Get(int Tick, int64_t *pTagtime, CSnapshot **ppData, CSnapshot **ppAltData)
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

// CSnapshotBuilder
CSnapshotBuilder::CSnapshotBuilder()
{
	m_NumExtendedItemTypes = 0;
}

void CSnapshotBuilder::Init(bool Sixup)
{
	m_DataSize = 0;
	m_NumItems = 0;
	m_Sixup = Sixup;

	for(int i = 0; i < m_NumExtendedItemTypes; i++)
	{
		AddExtendedItemType(i);
	}
}

CSnapshotItem *CSnapshotBuilder::GetItem(int Index)
{
	return (CSnapshotItem *)&(m_aData[m_aOffsets[Index]]);
}

int *CSnapshotBuilder::GetItemData(int Key)
{
	for(int i = 0; i < m_NumItems; i++)
	{
		if(GetItem(i)->Key() == Key)
			return GetItem(i)->Data();
	}
	return 0;
}

int CSnapshotBuilder::Finish(void *pSnapData)
{
	// flatten and make the snapshot
	CSnapshot *pSnap = (CSnapshot *)pSnapData;
	pSnap->m_DataSize = m_DataSize;
	pSnap->m_NumItems = m_NumItems;
	mem_copy(pSnap->Offsets(), m_aOffsets, pSnap->OffsetSize());
	mem_copy(pSnap->DataStart(), m_aData, m_DataSize);
	return pSnap->TotalSize();
}

int CSnapshotBuilder::GetTypeFromIndex(int Index)
{
	return CSnapshot::MAX_TYPE - Index;
}

void CSnapshotBuilder::AddExtendedItemType(int Index)
{
	dbg_assert(0 <= Index && Index < m_NumExtendedItemTypes, "index out of range");
	int TypeID = m_aExtendedItemTypes[Index];
	CUuid Uuid = g_UuidManager.GetUuid(TypeID);
	int *pUuidItem = (int *)NewItem(0, GetTypeFromIndex(Index), sizeof(Uuid)); // NETOBJTYPE_EX
	if(pUuidItem)
	{
		for(int i = 0; i < (int)sizeof(CUuid) / 4; i++)
			pUuidItem[i] = bytes_be_to_int(&Uuid.m_aData[i * 4]);
	}
}

int CSnapshotBuilder::GetExtendedItemTypeIndex(int TypeID)
{
	for(int i = 0; i < m_NumExtendedItemTypes; i++)
	{
		if(m_aExtendedItemTypes[i] == TypeID)
		{
			return i;
		}
	}
	dbg_assert(m_NumExtendedItemTypes < MAX_EXTENDED_ITEM_TYPES, "too many extended item types");
	int Index = m_NumExtendedItemTypes;
	m_aExtendedItemTypes[Index] = TypeID;
	m_NumExtendedItemTypes++;
	return Index;
}

void *CSnapshotBuilder::NewItem(int Type, int ID, int Size)
{
	if(ID == -1)
	{
		return 0;
	}

	if(m_DataSize + sizeof(CSnapshotItem) + Size >= CSnapshot::MAX_SIZE ||
		m_NumItems + 1 >= CSnapshot::MAX_ITEMS)
	{
		dbg_assert(m_DataSize < CSnapshot::MAX_SIZE, "too much data");
		dbg_assert(m_NumItems < CSnapshot::MAX_ITEMS, "too many items");
		return 0;
	}

	bool Extended = false;
	if(Type >= OFFSET_UUID)
	{
		Extended = true;
		Type = GetTypeFromIndex(GetExtendedItemTypeIndex(Type));
	}

	CSnapshotItem *pObj = (CSnapshotItem *)(m_aData + m_DataSize);

	if(m_Sixup && !Extended)
	{
		if(Type >= 0)
			Type = Obj_SixToSeven(Type);
		else
			Type *= -1;

		if(Type < 0)
			return pObj;
	}

	mem_zero(pObj, sizeof(CSnapshotItem) + Size);
	pObj->m_TypeAndID = (Type << 16) | ID;
	m_aOffsets[m_NumItems] = m_DataSize;
	m_DataSize += sizeof(CSnapshotItem) + Size;
	m_NumItems++;

	return pObj->Data();
}
