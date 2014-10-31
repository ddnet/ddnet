#ifndef ENGINE_FETCHER_H
#define ENGINE_FETCHER_H

#include "kernel.h"

typedef void (*PROGFUNC)(unsigned JobNum, unsigned DlTotal, unsigned DlCurr, unsigned UlTotal, unsigned Dltotal);

class IFetcher : public IInterface
{
	MACRO_INTERFACE("fetcher", 0)
public:
	virtual bool Init() = 0;
	virtual int QueueAdd(char *pUrl, char *pDest, PROGFUNC pfnProgCb) = 0;
};

#endif
