#ifndef ENGINE_CLIENT_DEMOEDIT_H
#define ENGINE_CLIENT_DEMOEDIT_H

#include <engine/shared/demo.h>
#include <engine/shared/jobs.h>
#include <engine/shared/snapshot.h>

class IStorage;

class CDemoEdit : public IJob
{
	CSnapshotDelta m_SnapshotDelta;
	IStorage *m_pStorage;

	CDemoEditor m_DemoEditor;

	char m_aDemo[256];
	char m_aDst[256];
	int m_StartTick;
	int m_EndTick;
	bool m_Success;

public:
	CDemoEdit(const char *pNetVersion, CSnapshotDelta *pSnapshotDelta, IStorage *pStorage, const char *pDemo, const char *pDst, int StartTick, int EndTick);
	void Run() override;
	char *Destination() { return m_aDst; }
	bool Success() { return m_Success; }
};
#endif
