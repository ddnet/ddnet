#include "demoedit.h"
#include <engine/shared/demo.h>

CDemoEdit::CDemoEdit(const char *pNetVersion, class CSnapshotDelta *pSnapshotDelta, IStorage *pStorage, const char *pDemo, const char *pDst, int StartTick, int EndTick) :
	m_SnapshotDelta(*pSnapshotDelta),
	m_pStorage(pStorage)
{
	str_copy(m_aDemo, pDemo, sizeof(m_aDemo));
	str_copy(m_aDst, pDst, sizeof(m_aDst));

	m_StartTick = StartTick;
	m_EndTick = EndTick;

	// Init the demoeditor
	m_DemoEditor.Init(pNetVersion, &m_SnapshotDelta, NULL, pStorage);
}

void CDemoEdit::Run()
{
	// Slice the actual demo
	m_DemoEditor.Slice(m_aDemo, m_aDst, m_StartTick, m_EndTick, NULL, 0);
	// We remove the temporary demo file
	m_pStorage->RemoveFile(m_aDemo, IStorage::TYPE_SAVE);
}
