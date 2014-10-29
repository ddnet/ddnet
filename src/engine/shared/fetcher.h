#ifndef ENGINE_SHARED_FETCHER_H
#define ENGINE_SHARED_FETCHER_H

#include "curl/curl.h"
#include "curl/easy.h"
#include <engine/fetcher.h>

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

	int QueueAdd(char *pUrl, char *pDest, PROGFUNC pfnProgCb);
	static void FetcherThread(void *pUser);
	void FetchFile(CFetchTask *pTask);
	static void WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile);
};

#endif
