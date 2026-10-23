#include "save_job.h"

#include <base/log.h>
#include <base/str.h>

#include <engine/storage.h>

void CDataFileWriterFinishJob::Run()
{
	m_Writer.Finish();
	if(!m_pStorage->RenameFile(m_aTempFilename, m_aRealFilename, IStorage::TYPE_SAVE))
	{
		str_format(m_aErrorMessage, sizeof(m_aErrorMessage), "Saving failed: Could not move temporary map file '%s' to '%s'.", m_aTempFilename, m_aRealFilename);
		log_error("editor/save", "%s", m_aErrorMessage);
		return;
	}
	log_trace("editor/save", "Saved map to '%s'.", m_aRealFilename);
}

CDataFileWriterFinishJob::CDataFileWriterFinishJob(IStorage *pStorage, const char *pTempFilename, CDataFileWriter &&Writer, CEditorSaveState::CTicket Ticket) :
	m_pStorage(pStorage),
	m_Writer(std::move(Writer)),
	m_Ticket(std::move(Ticket))
{
	// Projection has copied the revision into the native writer. The job only
	// retains the destination, content marker and original map lifetime.
	m_Ticket.m_pRoot.reset();
	str_copy(m_aRealFilename, m_Ticket.m_Destination.c_str());
	str_copy(m_aTempFilename, pTempFilename);
	m_aErrorMessage[0] = '\0';
}
