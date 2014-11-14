#include <base/system.h>
#include "fetcher.h"

CFetchTask::CFetchTask()
{
	m_pNext = NULL;
	m_pUrl = NULL;
	m_pDest = NULL;
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

int CFetcher::QueueAdd(const char *pUrl, const char *pDest, PROGFUNC pfnProgCb)
{
	CFetchTask *pTask = new CFetchTask;
	pTask->m_pUrl = pUrl;
	pTask->m_pDest = pDest;
	pTask->m_pfnProgressCallback = pfnProgCb;
	pTask->m_Num = m_NumTasks++;

	dbg_msg("fetcher", "Main Waiting for lock");
	lock_wait(m_Lock);
	dbg_msg("fetcher", "Main Got lock");
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
	dbg_msg("fetcher", "Main Released lock");

	return pTask->m_Num;
}

void CFetcher::FetcherThread(void *pUser)
{
	CFetcher *pFetcher = (CFetcher *) pUser;
	dbg_msg("fetcher", "Thread start");
	while(1){
		dbg_msg("fetcher", "Thread Waiting for lock");
		lock_wait(pFetcher->m_Lock);
			dbg_msg("fetcher", "Thread Got lock");
			CFetchTask *pTask = pFetcher->m_pFirst;
			pFetcher->m_pFirst = pTask->m_pNext;
		lock_release(pFetcher->m_Lock);
		dbg_msg("fetcher", "Thread Released lock");
		if(pTask){
			dbg_msg("fetcher", "Task got %s", pTask->m_pUrl);
			pFetcher->FetchFile(pTask);
		}
		else{
			dbg_msg("fetcher", "No Task");
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
		curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSDATA, pTask);
		curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSFUNCTION, &CFetchTask::ProgressCallback);
	}
	curl_easy_perform(m_pHandle);
}

void CFetcher::WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile)
{
	io_write((IOHANDLE)pFile, pData, size*nmemb);
}

int CFetchTask::ProgressCallback(void *pUser, curl_off_t DlTotal, curl_off_t DlCurr, curl_off_t UlTotal, curl_off_t UlCurr)
{
	CFetchTask *pTask = (CFetchTask *)pUser;
	pTask->m_pfnProgressCallback(pTask->m_Num, DlTotal, DlCurr, UlTotal, UlCurr);
	return 0;
}
