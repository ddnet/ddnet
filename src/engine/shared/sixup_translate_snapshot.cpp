#include <base/system.h>

#include "snapshot.h"

void CSnapshotBuilder::Init7(const CSnapshot *pSnapshot)
{
	// the method is called Init7 because it is only used for 0.7 support
	// but the snap we are building is a 0.6 snap
	m_Sixup = false;

	if(pSnapshot->m_DataSize + sizeof(CSnapshot) + pSnapshot->m_NumItems * sizeof(int) * 2 > CSnapshot::MAX_SIZE || pSnapshot->m_NumItems > CSnapshot::MAX_ITEMS)
	{
		// key and offset per item
		dbg_assert(m_DataSize + sizeof(CSnapshot) + m_NumItems * sizeof(int) * 2 < CSnapshot::MAX_SIZE, "too much data");
		dbg_assert(m_NumItems < CSnapshot::MAX_ITEMS, "too many items");
		dbg_msg("sixup", "demo recording failed on invalid snapshot");
		m_DataSize = 0;
		m_NumItems = 0;
		return;
	}

	m_DataSize = pSnapshot->m_DataSize;
	m_NumItems = pSnapshot->m_NumItems;
	mem_copy(m_aOffsets, pSnapshot->Offsets(), sizeof(int) * m_NumItems);
	mem_copy(m_aData, pSnapshot->DataStart(), m_DataSize);
}
