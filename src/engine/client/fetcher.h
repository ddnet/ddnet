#ifndef ENGINE_CLIENT_FETCHER_H
#define ENGINE_CLIENT_FETCHER_H

#define WIN32_LEAN_AND_MEAN
#include "curl/curl.h"
#include "curl/easy.h"
#include <engine/fetcher.h>

class CFetcher : public IFetcher
{
private:
	CURL *m_pHandle;

	void *m_pThHandle;

	LOCK m_Lock;
	CFetchTask *m_pFirst;
	CFetchTask *m_pLast;
	class IStorage *m_pStorage;
public:
	CFetcher();
	virtual bool Init();
	~CFetcher();

	virtual void QueueAdd(CFetchTask *pTask, const char *pUrl, const char *pDest, int StorageType = 2, void *pUser = 0, COMPFUNC pfnCompCb = 0, PROGFUNC pfnProgCb = 0);
	virtual void Escape(char *pBud, size_t size, const char *pStr);
	static void FetcherThread(void *pUser);
	void FetchFile(CFetchTask *pTask);
	static void WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile);
	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
};

#endif
