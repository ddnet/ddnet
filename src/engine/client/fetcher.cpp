#include <base/system.h>
#include "fetcher.h"

CFetcher::CFetcher()
{
	m_pHandle = NULL;
	m_Lock = lock_create();
	m_pFirst = NULL;
	m_pLast = NULL;
	m_NumTasks = 0;
}

CFetchTask::CFetchTask()
{
	m_pNext = NULL;
	m_pUrl = NULL;
	m_pDest = NULL;
}

bool CFetcher::Init()
{
	if(!curl_global_init(CURL_GLOBAL_DEFAULT) && (m_pHandle = curl_easy_init()))
		return true;
	return false;
}

int CFetcher::QueueAdd(char *pUrl, char *pDest, PROGFUNC pfnProgCb)
{
	CFetchTask *pTask = new CFetchTask;
	pTask->m_pUrl = pUrl;
	pTask->m_pDest = pDest;
	pTask->m_pfnProgressCallback = pfnProgCb;
	pTask->m_Num = m_NumTasks++;

	lock_wait(m_Lock);
	if(!m_pFirst){
		thread_create(&FetcherThread, this);
		m_pFirst = pTask;
	}
	m_pLast->m_pNext = pTask;
	m_pLast = pTask;
	lock_release(m_Lock);

	return pTask->m_Num;
}

void CFetcher::FetcherThread(void *pUser)
{
	CFetcher *pFetcher = (CFetcher *) pUser;
	lock_wait(pFetcher->m_Lock);
		CFetchTask *pTask = pFetcher->m_pFirst;
		pFetcher->m_pFirst = pTask->m_pNext;
	lock_release(pFetcher->m_Lock);
	pFetcher->FetchFile(pTask);
}

void CFetcher::FetchFile(CFetchTask *pTask)
{
	IOHANDLE File = io_open(pTask->m_pDest, IOFLAG_WRITE);

	curl_easy_setopt(m_pHandle, CURLOPT_URL, pTask->m_pUrl);
	curl_easy_setopt(m_pHandle, CURLOPT_WRITEDATA, File);
	curl_easy_setopt(m_pHandle, CURLOPT_WRITEFUNCTION, &CFetcher::WriteToFile);
	curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSDATA, pTask);
	curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSFUNCTION, &CFetchTask::ProgressCallback);
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
