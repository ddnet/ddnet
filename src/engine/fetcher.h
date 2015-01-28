#ifndef ENGINE_FETCHER_H
#define ENGINE_FETCHER_H

#include "kernel.h"

class CFetchTask;

typedef void (*PROGFUNC)(CFetchTask *pTask, void *pUser);
typedef void (*COMPFUNC)(CFetchTask *pDest, void *pUser);

class CFetchTask
{
	friend class CFetcher;

	CFetchTask *m_pNext;

	char m_pUrl[256];
	char m_pDest[128];
	int m_StorageType;
	PROGFUNC m_pfnProgressCallback;
	COMPFUNC m_pfnCompCallback;
	void *m_pUser;

	double m_Current;
	double m_Size;
	int m_Progress;
	int m_State;
	bool m_Abort;
public:	
	CFetchTask();

	enum
	{
		STATE_ERROR = -1,
		STATE_QUEUED,
		STATE_RUNNING,
		STATE_DONE,
		STATE_ABORTED,
	};

	const double Current() const { return m_Current; };
	const double Size() const { return m_Size; };
	const int Progress() const { return m_Progress; };
	const int State() const { return m_State; };
	const char *Dest() const { return m_pDest; };

	void Abort() { m_Abort = true; };
};

class IFetcher : public IInterface
{
	MACRO_INTERFACE("fetcher", 0)
public:
	virtual bool Init() = 0;
	virtual void QueueAdd(CFetchTask *pTask,const char *pUrl, const char *pDest, int StorageType = 2, void *pUser = 0, COMPFUNC pfnCompCb = 0, PROGFUNC pfnProgCb = 0) = 0;
	virtual long HTTPResponse(const char *pUrl) = 0;
};

#endif
