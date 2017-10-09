#ifndef ENGINE_FETCHER_H
#define ENGINE_FETCHER_H

#include <engine/shared/jobs.h>
#include "kernel.h"
#include "stddef.h"

class IFetchTask
{
public:
	enum
	{
		STATE_ERROR = -1,
		STATE_QUEUED,
		STATE_RUNNING,
		STATE_DONE,
		STATE_ABORTED,
	};

	virtual ~IFetchTask() {};

	virtual double Current() = 0;
	virtual double Size() = 0;
	virtual int Progress() = 0;
	virtual int State() = 0;
	virtual const char *Dest() = 0;
	virtual void Abort() = 0;

	virtual void Destroy() = 0;
};

typedef void (*PROGFUNC)(IFetchTask *pTask, void *pUser);
typedef void (*COMPFUNC)(IFetchTask *pDest, void *pUser);

class IFetcher : public IInterface
{
	MACRO_INTERFACE("fetcher", 0)
public:
	virtual bool Init() = 0;
	virtual IFetchTask *FetchFile(const char *pUrl, const char *pDest, int StorageType = -2, bool UseDDNetCA = false, bool CanTimeout = true, void *pUser = 0, COMPFUNC pfnCompCb = 0, PROGFUNC pfnProgCb = 0) = 0;
	virtual void Escape(char *pBud, size_t size, const char *pStr) = 0;
};

#endif
