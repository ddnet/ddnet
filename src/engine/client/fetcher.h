#ifndef ENGINE_CLIENT_FETCHER_H
#define ENGINE_CLIENT_FETCHER_H

#include <engine/fetcher.h>

class CFetcher : public IFetcher
{
private:
	class IEngine *m_pEngine;
	class IStorage *m_pStorage;

public:
	virtual bool Init();

	virtual IFetchTask *FetchFile(const char *pUrl, const char *pDest, int StorageType = -2, bool UseDDNetCA = false, bool CanTimeout = true, void *pUser = 0, COMPFUNC pfnCompCb = 0, PROGFUNC pfnProgCb = 0);
	virtual void Escape(char *pBud, size_t size, const char *pStr);

	static int FetcherThread(void *pUser);
	static size_t WriteToFile(char *pData, size_t size, size_t nmemb, void *pFile);
	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
};

#endif
