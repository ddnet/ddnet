#ifndef ENGINE_FETCHER_H
#define ENGINE_FETCHER_H

#include "kernel.h"

typedef void (*PROGFUNC)(unsigned JobNum, unsigned DlTotal, unsigned DlCurr, unsigned UlTotal, unsigned Dltotal);

class CFetchTask
{
private:
	static int ProgressCallback(void *pUser, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);
public:	
	CFetchTask *m_pNext;
	char *m_pUrl;
	char *m_pDest;
	PROGFUNC m_pfnProgressCallback;
	CFetchTask();
};

class IFetcher : public IInterface
{
	MACRO_INTERFACE("fetcher", 0)
public:
	virtual bool Init() = 0;
	virtual int QueueAdd(char *pUrl, char *pDest, PROGFUNC pfnProgCb) = 0;
};

#endif
