#include <base/system.h>
#include <engine/storage.h>
#include "fetcher.h"

CFetchTask::CFetchTask()
{
	m_pNext = NULL;
}

CFetcher::CFetcher()
{
	m_pStorage = NULL;
	m_pHandle = NULL;
	m_Lock = lock_create();
	m_pFirst = NULL;
	m_pLast = NULL;
}

bool CFetcher::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
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

void CFetcher::QueueAdd(CFetchTask *pTask,const char *pUrl, const char *pDest, int StorageType, void *pUser, COMPFUNC pfnCompCb, PROGFUNC pfnProgCb)
{
	str_copy(pTask->m_pUrl, pUrl, sizeof(pTask->m_pUrl));
	str_copy(pTask->m_pDest, pDest, sizeof(pTask->m_pDest));
	pTask->m_StorageType = StorageType;
	pTask->m_pfnProgressCallback = pfnProgCb;
	pTask->m_pfnCompCallback = pfnCompCb;
	pTask->m_pUser = pUser;
	pTask->m_Size = pTask->m_Progress = pTask->m_RespCode = 0;

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
	pTask->m_State = CFetchTask::STATE_QUEUED;
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
		}
		else{
			dbg_msg("fetcher", "No task. Killing the thread...");
			break;
		}
	}
}

bool CFetcher::FetchFile(CFetchTask *pTask)
{
	for(int i = 0; pTask->m_pDest[i] != '\0'; i++){
		if(pTask->m_pDest[i] == '/'){
			pTask->m_pDest[i] = '\0';
			m_pStorage->CreateFolder(pTask->m_pDest,2);
			pTask->m_pDest[i] = '/';
		}
	}

	IOHANDLE File = m_pStorage->OpenFile(pTask->m_pDest, IOFLAG_WRITE, pTask->m_StorageType);

	char aErr[CURL_ERROR_SIZE];
	curl_easy_setopt(m_pHandle, CURLOPT_ERRORBUFFER, aErr);

	curl_easy_setopt(m_pHandle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(m_pHandle, CURLOPT_CAINFO, "cacert.pem");
	curl_easy_setopt(m_pHandle, CURLOPT_URL, pTask->m_pUrl);
	curl_easy_setopt(m_pHandle, CURLOPT_WRITEDATA, File);
	curl_easy_setopt(m_pHandle, CURLOPT_WRITEFUNCTION, &CFetcher::WriteToFile);
	curl_easy_setopt(m_pHandle, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSDATA, pTask);
	curl_easy_setopt(m_pHandle, CURLOPT_PROGRESSFUNCTION, &CFetcher::ProgressCallback);

	dbg_msg("fetcher", "Downloading %s", pTask->m_pDest);
	pTask->m_State = CFetchTask::STATE_RUNNING;
	if(curl_easy_perform(m_pHandle) != CURLE_OK){
		dbg_msg("fetcher", "Task failed. libcurl error: %s", aErr);
		pTask->m_State = CFetchTask::STATE_ERROR;
		return false;
	}
	curl_easy_getinfo(m_pHandle, CURLINFO_RESPONSE_CODE, &pTask->m_RespCode);
	pTask->m_State = CFetchTask::STATE_DONE;
	io_close(File);
	if(pTask->m_pfnCompCallback)
		pTask->m_pfnCompCallback(pTask, pTask->m_pUser);
	dbg_msg("fetcher", "Task done %s", pTask->m_pDest);
	return true;
}

void CFetcher::WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile)
{
	io_write((IOHANDLE)pFile, pData, size*nmemb);
}

int CFetcher::ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr)
{
	CFetchTask *pTask = (CFetchTask *)pUser;
	dbg_msg("fetcher", "DlCurr:%f, DlTotal:%f", DlCurr, DlTotal);
	if(!pTask->m_Size)
		pTask->m_Size = DlTotal;
	pTask->m_Progress = (100*DlCurr)/(DlTotal ? DlTotal : 1);
	if(pTask->m_pfnProgressCallback)
		pTask->m_pfnProgressCallback(pTask, pTask->m_pUser);
	return 0;
}
