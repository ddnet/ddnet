#ifndef ENGINE_FETCHER_H
#define ENGINE_FETCHER_H

#include "kernel.h"

typedef void (*PROGFUNC)(const char *pDest, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
typedef void (*COMPFUNC)(const char *pDest);

class IFetcher : public IInterface
{
	MACRO_INTERFACE("fetcher", 0)
public:
	virtual bool Init() = 0;
	virtual void QueueAdd(const char *pUrl, const char *pDest, COMPFUNC pfnCompCb = NULL, PROGFUNC pfnProgCb = NULL) = 0;
};

#endif
