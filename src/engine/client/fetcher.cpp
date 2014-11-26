#include <base/system.h>
#include "fetcher.h"

CFetchTask::CFetchTask()
{
	m_pNext = NULL;
}

CFetcher::CFetcher()
{
	m_pHandle = NULL;
	m_Lock = lock_create();
	m_pFirst = NULL;
	m_pLast = NULL;
	m_NumTasks = 0;
}

bool CFetcher::Init()
{
	if(!curl_global_init(CURL_GLOBAL_DEFAULT) && (m_pHandle = curl_easy_init()))
		return true;
	return false;
}

CFetcher::~CFetcher()
{
	if(m_pHandle)
		curl_easy_cleanup(m_pHandle);
	curl_global_cleanup();
}

void CFetcher::QueueAdd(const char *pUrl, const char *pDest, PROGFUNC pfnProgCb)
{
	CFetchTask *pTask = new CFetchTask;
	str_copy(pTask->m_pUrl, pUrl, sizeof(pTask->m_pUrl));
	str_copy(pTask->m_pDest, pDest, sizeof(pTask->m_pDest));
	pTask->m_pfnProgressCallback = pfnProgCb;

	lock_wait(m_Lock);
	if(!m_pFirst){
		void *pHandle = thread_create(&FetcherThread, this);
		thread_detach(pHandle);
		m_pFirst = pTask;
		m_pLast = m_pFirst;
	}
	else {
		m_pLast->m_pNext = pTask;
		m_pLast = pTask;
	}
	lock_release(m_Lock);
}

void CFetcher::FetcherThread(void *pUser)
{
	CFetcher *pFetcher = (CFetcher *) pUser;
	dbg_msg("fetcher", "Thread started...");
	while(1){
		lock_wait(pFetcher->m_Lock);
			CFetchTask *pTask = pFetcher->m_pFirst;
			if(pTask)
				pFetcher->m_pFirst = pTask->m_pNext;
		lock_release(pFetcher->m_Lock);
		if(pTask){
			dbg_msg("fetcher", "Task got %s:%s", pTask->m_pUrl, pTask->m_pDest);
			pFetcher->FetchFile(pTask);
			delete pTask;
		}
		else{
			dbg_msg("fetcher", "No task. Killing the thread...");
			break;
		}
	}
}

void CFetcher::FetchFile(CFetchTask *pTask)
{
	IOHANDLE File = io_open(pTask->m_pDest, IOFLAG_WRITE);

	curl_easy_setopt(m_pHandle, CURLOPT_URL, pTask->m_pUrl);
	curl_easy_setopt(m_pHandle, CURLOPT_WRITEDATA, File);
	curl_easy_setopt(m_pHandle, CURLOPT_WRITEFUNCTION, &CFetcher::WriteToFile);
	if(pTask->m_pfnProgressCallback){
		curl_easy_setopt(m_pHandle, CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSDATA, pTask);
		curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSFUNCTION, &CFetchTask::ProgressCallback);
	}
	dbg_msg("fetcher", "Downloading %s", pTask->m_pDest);
	curl_easy_perform(m_pHandle);
	dbg_msg("fetcher", "Task done %s", pTask->m_pDest);
}

void CFetcher::WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile)
{
	io_write((IOHANDLE)pFile, pData, size*nmemb);
}

int CFetchTask::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CFetchTask *pTask = (CFetchTask *)pUser;
	pTask->m_pfnProgressCallback(pTask->m_pDest, DlTotal, DlCurr, UlTotal, UlCurr);
	return 0;
}
