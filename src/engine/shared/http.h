#ifndef ENGINE_SHARED_HTTP_H
#define ENGINE_SHARED_HTTP_H

#include <engine/shared/jobs.h>
#include <engine/storage.h>
#include <engine/kernel.h>

typedef struct _json_value json_value;

enum
{
	HTTP_ERROR = -1,
	HTTP_QUEUED,
	HTTP_RUNNING,
	HTTP_DONE,
	HTTP_ABORTED,
};

class CRequest : public IJob
{
	// Abort the request with an error if `BeforeInit()` or `AfterInit()`
	// returns something nonzero. Also abort the request if `OnData()`
	// returns something other than `DataSize`.
	virtual int BeforeInit() { return 0; }
	virtual int AfterInit(void *pCurl) { return 0; }
	virtual size_t OnData(char *pData, size_t DataSize) = 0;

	virtual void OnProgress() { }
	virtual void BeforeCompletion() { }
	virtual void OnCompletion() { }

	char m_aUrl[256];
	bool m_CanTimeout;

	double m_Size;
	double m_Current;
	int m_Progress;

	std::atomic<int> m_State;
	std::atomic<bool> m_Abort;

	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
	static size_t WriteCallback(char *pData, size_t Size, size_t Number, void *pUser);

	void Run();

public:
	CRequest(const char *pUrl, bool CanTimeout);

	double Current() const { return m_Current; }
	double Size() const { return m_Size; }
	int Progress() const { return m_Progress; }
	int State() const { return m_State; }
	void Abort() { m_Abort = true; }
};

class CGet : public CRequest
{
	virtual size_t OnData(char *pData, size_t DataSize);

	size_t m_BufferSize;
	size_t m_BufferLength;
	unsigned char *m_pBuffer;

public:
	CGet(const char *pUrl, bool CanTimeout);
	~CGet();

	size_t ResultSize() const { if(!Result()) { return 0; } else { return m_BufferSize; } }
	unsigned char *Result() const;
	unsigned char *TakeResult();
	json_value *ResultJson() const;
};

class CGetFile : public CRequest
{
	virtual size_t OnData(char *pData, size_t DataSize);
	virtual int BeforeInit();
	virtual void BeforeCompletion();

	IStorage *m_pStorage;

	char m_aDest[256];
	int m_StorageType;
	IOHANDLE m_File;

public:
	CGetFile(IStorage *pStorage, const char *pUrl, const char *pDest, int StorageType = -2, bool CanTimeout = true);

	const char *Dest() const { return m_aDest; }
};


class CPostJson : public CRequest
{
	virtual size_t OnData(char *pData, size_t DataSize) { return DataSize; }
	virtual int AfterInit(void *pCurl);

	char m_aJson[1024];

public:
	CPostJson(const char *pUrl, bool CanTimeout, const char *pJson);
};

bool HttpInit(IStorage *pStorage);
void EscapeUrl(char *pBuf, int Size, const char *pStr);
char *EscapeJson(char *pBuffer, int BufferSize, const char *pString);
const char *JsonBool(bool Bool);
#endif // ENGINE_SHARED_HTTP_H
