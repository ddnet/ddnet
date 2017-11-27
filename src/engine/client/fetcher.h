#ifndef ENGINE_CLIENT_FETCHER_H
#define ENGINE_CLIENT_FETCHER_H

#include <engine/shared/jobs.h>
#include <engine/storage.h>
#include <engine/kernel.h>

class CFetchTask : public IJob
{
private:
	IStorage *m_pStorage;

	char m_aUrl[256];
	char m_aDest[128];
	int m_StorageType;
	bool m_UseDDNetCA;
	bool m_CanTimeout;

	double m_Size;
	double m_Current;
	int m_Progress;
	int m_State;

	std::atomic<bool> m_Abort;

	virtual void OnProgress() { }
	virtual void OnCompletion() { }

	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
	static size_t WriteCallback(char *pData, size_t Size, size_t Number, void *pFile);

	void Run();

public:
	enum
	{
		STATE_ERROR = -1,
		STATE_QUEUED,
		STATE_RUNNING,
		STATE_DONE,
		STATE_ABORTED,
	};

	CFetchTask(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType = -2, bool UseDDNetCA = false, bool CanTimeout = true);

	double Current() const;
	double Size() const;
	int Progress() const;
	int State() const;
	const char *Dest() const;
	void Abort();
};

bool FetcherInit();
void EscapeUrl(char *pBud, int Size, const char *pStr);
#endif
