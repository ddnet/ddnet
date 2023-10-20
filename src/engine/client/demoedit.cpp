#include "demoedit.h"

#include <engine/client/client.h>
#include <engine/console.h>
#include <engine/shared/demo.h>
#include <engine/storage.h>

#include <game/localization.h>

CDemoEdit::CDemoEdit(CClient *pClient, const char *pNetVersion, class CSnapshotDelta *pSnapshotDelta, const char *pDemo, const char *pDst, int StartTick, int EndTick) :
	m_pClient(pClient),
	m_SnapshotDelta(*pSnapshotDelta)
{
	str_copy(m_aDemo, pDemo);
	str_copy(m_aDst, pDst);

	m_StartTick = StartTick;
	m_EndTick = EndTick;

	// Init the demoeditor
	m_DemoEditor.Init(pNetVersion, &m_SnapshotDelta, NULL, pClient->Storage());
}

void CDemoEdit::Run()
{
	// Slice the current demo
	m_DemoEditor.Slice(m_aDemo, m_aDst, m_StartTick, m_EndTick, NULL, 0);
	// We remove the temporary demo file
	m_pClient->Storage()->RemoveFile(m_aDemo, IStorage::TYPE_SAVE);
}

void CDemoEdit::Done()
{
	if(m_pClient->State() != IClient::STATE_ONLINE)
		return;

	char aBuf[IO_MAX_PATH_LENGTH + 64];
	str_format(aBuf, sizeof(aBuf), "Successfully saved the replay to '%s'!", Destination());
	m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", aBuf);

	m_pClient->GameClient()->Echo(Localize("Successfully saved the replay!"));
}
