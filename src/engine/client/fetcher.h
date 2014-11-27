#ifndef ENGINE_CLIENT_FETCHER_H
#define ENGINE_CLIENT_FETCHER_H

#include "curl/curl.h"
#include "curl/easy.h"
#include <engine/fetcher.h>

class CFetchTask
{
public:	
	CFetchTask *m_pNext;
	char m_pUrl[256];
	char m_pDest[128];
	unsigned m_Num;
	PROGFUNC m_pfnProgressCallback;
	COMPFUNC m_pfnCompCallback;
	void *m_pUser;
	CFetchTask();

	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
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

	void QueueAdd(const char *pUrl, const char *pDest, COMPFUNC pfnCompCb = NULL, PROGFUNC pfnProgCb = NULL, void *pUser = NULL);
	static void FetcherThread(void *pUser);
	void FetchFile(CFetchTask *pTask);
	static void WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile);
};

#endif
