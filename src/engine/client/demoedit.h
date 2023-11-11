#ifndef ENGINE_CLIENT_DEMOEDIT_H
#define ENGINE_CLIENT_DEMOEDIT_H

#include <engine/shared/demo.h>
#include <engine/shared/jobs.h>
#include <engine/shared/snapshot.h>

class CClient;

class CDemoEdit : public IJob
{
	CClient *m_pClient;
	CSnapshotDelta m_SnapshotDelta;

	CDemoEditor m_DemoEditor;

	char m_aDemo[256];
	char m_aDst[256];
	int m_StartTick;
	int m_EndTick;

	void Run() override;
	void Done() override;

public:
	CDemoEdit(CClient *pClient, const char *pNetVersion, CSnapshotDelta *pSnapshotDelta, const char *pDemo, const char *pDst, int StartTick, int EndTick);
	const char *Destination() const { return m_aDst; }
};
#endif
