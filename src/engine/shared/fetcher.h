#ifndef ENGINE_SHARED_FETCHER_H
#define ENGINE_SHARED_FETCHER_H

#include <engine/shared/jobs.h>
#include <engine/storage.h>
#include <engine/kernel.h>

enum
{
	HTTP_ERROR = -1,
	HTTP_QUEUED,
	HTTP_RUNNING,
	HTTP_DONE,
	HTTP_ABORTED,
};

class CGetFile : public IJob
{
private:
	IStorage *m_pStorage;

	char m_aUrl[256];
	char m_aDest[256];
	int m_StorageType;
	bool m_UseDDNetCA;
	bool m_CanTimeout;

	double m_Size;
	double m_Current;
	int m_Progress;
	std::atomic<int> m_State;

	std::atomic<bool> m_Abort;

	virtual void OnProgress() { }
	virtual void OnCompletion() { }

	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
	static size_t WriteCallback(char *pData, size_t Size, size_t Number, void *pFile);

	void Run();

public:
	CGetFile(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType = -2, bool UseDDNetCA = false, bool CanTimeout = true);

	double Current() const;
	double Size() const;
	int Progress() const;
	int State() const;
	const char *Dest() const;
	void Abort();
};


class CPostJson : public IJob
{
private:
	char m_aUrl[256];
	char m_aJson[1024];
	std::atomic<int> m_State;

	void Run();

	virtual void OnCompletion() { }

public:
	CPostJson(const char *pUrl, const char *pJson);
	int State() const;
};

bool FetcherInit();
void EscapeUrl(char *pBuf, int Size, const char *pStr);
char *EscapeJson(char *pBuffer, int BufferSize, const char *pString);
#endif // ENGINE_SHARED_FETCHER_H
