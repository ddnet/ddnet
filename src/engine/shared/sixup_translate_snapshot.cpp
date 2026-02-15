#include "snapshot.h"

#include <base/system.h>

void CSnapshotBuilder::Init7(const CSnapshot *pSnapshot)
{
	dbg_assert(!m_Building, "Snapshot builder is already building snapshot. Call `Finish` for each call to `Init`.");

	// the method is called Init7 because it is only used for 0.7 support
	// but the snap we are building is a 0.6 snap
	m_Building = true;
	m_Sixup = false;

	m_DataSize = pSnapshot->m_DataSize;
	m_NumItems = pSnapshot->m_NumItems;
	mem_copy(m_aOffsets, pSnapshot->Offsets(), sizeof(int) * m_NumItems);
	mem_copy(m_aData, pSnapshot->DataStart(), m_DataSize);
}
