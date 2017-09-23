#ifndef ENGINE_FETCHER_H
#define ENGINE_FETCHER_H

#include <engine/shared/jobs.h>
#include "kernel.h"
#include "stddef.h"

#define WIN32_LEAN_AND_MEAN
#include "curl/curl.h"

class CFetchTask;

typedef void (*PROGFUNC)(CFetchTask *pTask, void *pUser);
typedef void (*COMPFUNC)(CFetchTask *pDest, void *pUser);

class CFetchTask
{
	friend class CFetcher;
	class CFetcher *m_pFetcher;

	CJob m_Job;
	CURL *m_pHandle;
	void *m_pUser;

	char m_aUrl[256];
	char m_aDest[128];
	int m_StorageType;
	bool m_UseDDNetCA;
	bool m_CanTimeout;

	PROGFUNC m_pfnProgressCallback;
	COMPFUNC m_pfnCompCallback;

	double m_Size;
	double m_Current;
	int m_Progress;
	int m_State;

	bool m_Abort;
public:
	enum
	{
		STATE_ERROR = -1,
		STATE_QUEUED,
		STATE_RUNNING,
		STATE_DONE,
		STATE_ABORTED,
	};

	double Current() const { return m_Current; };
	double Size() const { return m_Size; };
	int Progress() const { return m_Progress; };
	int State() const { return m_State; };
	const char *Dest() const { return m_aDest; };

	void Abort() { m_Abort = true; };
};

class IFetcher : public IInterface
{
	MACRO_INTERFACE("fetcher", 0)
public:
	virtual bool Init() = 0;
	virtual void FetchFile(CFetchTask *pTask, const char *pUrl, const char *pDest, int StorageType = -2, bool UseDDNetCA = false, bool CanTimeout = true, void *pUser = 0, COMPFUNC pfnCompCb = 0, PROGFUNC pfnProgCb = 0) = 0;
	virtual void Escape(char *pBud, size_t size, const char *pStr) = 0;
};

#endif
