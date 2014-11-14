#ifndef ENGINE_SHARED_FETCHER_H
#define ENGINE_SHARED_FETCHER_H

#include "curl/curl.h"
#include "curl/easy.h"
#include <engine/fetcher.h>

class CFetchTask
{
public:	
	CFetchTask *m_pNext;
	const char *m_pUrl;
	const char *m_pDest;
	unsigned m_Num;
	PROGFUNC m_pfnProgressCallback;
	CFetchTask();

	static int ProgressCallback(void *pUser, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);
};

class CFetcher : public IFetcher
{
private:
	CURL *m_pHandle;

	LOCK m_Lock;
	CFetchTask *m_pFirst;
	CFetchTask *m_pLast;
	unsigned m_NumTasks;
public:
	CFetcher();
	bool Init();
	~CFetcher();

	int QueueAdd(const char *pUrl, const char *pDest, PROGFUNC pfnProgCb = NULL);
	static void FetcherThread(void *pUser);
	void FetchFile(CFetchTask *pTask);
	static void WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile);
};

#endif
