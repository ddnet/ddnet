#ifndef GAME_EDITOR_HISTORY_SAVE_JOB_H
#define GAME_EDITOR_HISTORY_SAVE_JOB_H

#include "save_state.h"

#include <base/io.h>

#include <engine/shared/datafile.h>
#include <engine/shared/jobs.h>

#include <game/editor/mapitems/document_graph.h>

using CEditorSaveState = editor_history::CSaveState<CEditorDocumentValues>;

/**
 * Finishes a foreground-prepared native writer without reading editor objects.
 * Completion applies the captured content marker to the original map lifetime.
 * Queuing a job alone does not mark that map saved.
 */
class CDataFileWriterFinishJob : public IJob
{
	IStorage *m_pStorage;
	CDataFileWriter m_Writer;
	char m_aRealFilename[IO_MAX_PATH_LENGTH];
	char m_aTempFilename[IO_MAX_PATH_LENGTH];
	char m_aErrorMessage[2 * IO_MAX_PATH_LENGTH + 128];
	CEditorSaveState::CTicket m_Ticket;
	std::string m_AutosaveName;
	int m_AutosaveMax = 0;
	void Run() override;

public:
	CDataFileWriterFinishJob(IStorage *pStorage, const char *pTempFilename, CDataFileWriter &&Writer, CEditorSaveState::CTicket Ticket);
	void SetAutosaveName(std::string Name, int Maximum)
	{
		m_AutosaveName = std::move(Name);
		m_AutosaveMax = Maximum;
	}
	const std::string &AutosaveName() const { return m_AutosaveName; }
	int AutosaveMax() const { return m_AutosaveMax; }
	const char *RealFilename() const { return m_aRealFilename; }
	const char *ErrorMessage() const { return m_aErrorMessage; }
	const CEditorSaveState::CTicket &Ticket() const { return m_Ticket; }
};

#endif
