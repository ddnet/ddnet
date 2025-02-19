#ifndef ENGINE_SHARED_HTTP_H
#define ENGINE_SHARED_HTTP_H

#include <base/hash_ctxt.h>

#include <engine/shared/jobs.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <engine/http.h>

typedef struct _json_value json_value;
class IStorage;

enum class EHttpState
{
	ERROR = -1,
	QUEUED,
	RUNNING,
	DONE,
	ABORTED,
};

enum class HTTPLOG
{
	NONE,
	FAILURE,
	ALL,
};

enum class IPRESOLVE
{
	WHATEVER,
	V4,
	V6,
};

struct CTimeout
{
	long ConnectTimeoutMs;
	long TimeoutMs;
	long LowSpeedLimit;
	long LowSpeedTime;
};

class CHttpRequest : public IHttpRequest
{
	friend class CHttp;

	enum class REQUEST
	{
		GET = 0,
		HEAD,
		POST,
		POST_JSON,
	};

	static constexpr const char *GetRequestType(REQUEST Type)
	{
		switch(Type)
		{
		case REQUEST::GET:
			return "GET";
		case REQUEST::HEAD:
			return "HEAD";
		case REQUEST::POST:
		case REQUEST::POST_JSON:
			return "POST";
		}

		// Unreachable, maybe assert instead?
		return "UNKNOWN";
	}

	char m_aUrl[256] = {0};

	void *m_pHeaders = nullptr;
	unsigned char *m_pBody = nullptr;
	size_t m_BodyLength = 0;

	bool m_ValidateBeforeOverwrite = false;
	bool m_SkipByFileTime = true;

	CTimeout m_Timeout = CTimeout{0, 0, 0, 0};
	int64_t m_MaxResponseSize = -1;
	int64_t m_IfModifiedSince = -1;
	REQUEST m_Type = REQUEST::GET;

	SHA256_DIGEST m_ActualSha256 = SHA256_ZEROED;
	SHA256_CTX m_ActualSha256Ctx;
	SHA256_DIGEST m_ExpectedSha256 = SHA256_ZEROED;

	bool m_WriteToMemory = true;
	bool m_WriteToFile = false;

	uint64_t m_ResponseLength = 0;

	// If `m_WriteToMemory` is true.
	size_t m_BufferSize = 0;
	unsigned char *m_pBuffer = nullptr;

	// If `m_WriteToFile` is true.
	IOHANDLE m_File = nullptr;
	int m_StorageType = 0xdeadbeef;
	char m_aDestAbsoluteTmp[IO_MAX_PATH_LENGTH] = {0};
	char m_aDestAbsolute[IO_MAX_PATH_LENGTH] = {0};
	char m_aDest[IO_MAX_PATH_LENGTH] = {0};

	std::atomic<double> m_Size{0.0};
	std::atomic<double> m_Current{0.0};
	std::atomic<int> m_Progress{0};
	HTTPLOG m_LogProgress = HTTPLOG::ALL;
	IPRESOLVE m_IpResolve = IPRESOLVE::WHATEVER;

	bool m_FailOnErrorStatus = true;

	char m_aErr[256]; // 256 == CURL_ERROR_SIZE
	std::atomic<EHttpState> m_State{EHttpState::QUEUED};
	std::atomic<bool> m_Abort{false};
	std::mutex m_WaitMutex;
	std::condition_variable m_WaitCondition;

	int m_StatusCode = 0;
	bool m_HeadersEnded = false;
	std::optional<int64_t> m_ResultDate = {};
	std::optional<int64_t> m_ResultLastModified = {};

	bool ShouldSkipRequest();
	// Abort the request with an error if `BeforeInit()` returns false.
	bool BeforeInit();
	bool ConfigureHandle(void *pHandle); // void * == CURL *
	// `pHandle` can be nullptr if no handle was ever created for this request.
	void OnCompletionInternal(void *pHandle, unsigned int Result); // void * == CURL *, unsigned int == CURLcode

	// Abort the request if `OnHeader()` returns something other than
	// `DataSize`. `pHeader` is NOT null-terminated.
	size_t OnHeader(char *pHeader, size_t HeaderSize);
	// Abort the request if `OnData()` returns something other than
	// `DataSize`.
	size_t OnData(char *pData, size_t DataSize);

	static int ProgressCallback(void *pUser, double DlTotal, double DlCurr, double UlTotal, double UlCurr);
	static size_t HeaderCallback(char *pData, size_t Size, size_t Number, void *pUser);
	static size_t WriteCallback(char *pData, size_t Size, size_t Number, void *pUser);

protected:
	// These run on the curl thread now, DO NOT STALL THE THREAD
	virtual void OnProgress() {}
	virtual void OnCompletion(EHttpState State) {}

public:
	CHttpRequest(const char *pUrl);
	virtual ~CHttpRequest();

	void Timeout(CTimeout Timeout) { m_Timeout = Timeout; }
	// Skip the download if the local file is newer or as new as the remote file.
	void SkipByFileTime(bool SkipByFileTime) { m_SkipByFileTime = SkipByFileTime; }
	void MaxResponseSize(int64_t MaxResponseSize) { m_MaxResponseSize = MaxResponseSize; }
	void LogProgress(HTTPLOG LogProgress) { m_LogProgress = LogProgress; }
	void IpResolve(IPRESOLVE IpResolve) { m_IpResolve = IpResolve; }
	void FailOnErrorStatus(bool FailOnErrorStatus) { m_FailOnErrorStatus = FailOnErrorStatus; }
	// Download to memory only. Get the result via `Result*`.
	void WriteToMemory()
	{
		m_WriteToMemory = true;
		m_WriteToFile = false;
	}
	// Download to filesystem and memory.
	void WriteToFileAndMemory(IStorage *pStorage, const char *pDest, int StorageType);
	// Download to the filesystem only.
	void WriteToFile(IStorage *pStorage, const char *pDest, int StorageType);
	// Don't place the file in the specified location until
	// `OnValidation(true)` has been called.
	void ValidateBeforeOverwrite(bool ValidateBeforeOverwrite) { m_ValidateBeforeOverwrite = ValidateBeforeOverwrite; }
	void ExpectSha256(const SHA256_DIGEST &Sha256) { m_ExpectedSha256 = Sha256; }
	void Head() { m_Type = REQUEST::HEAD; }
	void Post(const unsigned char *pData, size_t DataLength)
	{
		m_Type = REQUEST::POST;
		m_BodyLength = DataLength;
		m_pBody = (unsigned char *)malloc(std::max((size_t)1, DataLength));
		mem_copy(m_pBody, pData, DataLength);
	}
	void PostJson(const char *pJson)
	{
		m_Type = REQUEST::POST_JSON;
		m_BodyLength = str_length(pJson);
		m_pBody = (unsigned char *)malloc(m_BodyLength);
		mem_copy(m_pBody, pJson, m_BodyLength);
	}
	void Header(const char *pNameColonValue);
	void HeaderString(const char *pName, const char *pValue)
	{
		char aHeader[256];
		str_format(aHeader, sizeof(aHeader), "%s: %s", pName, pValue);
		Header(aHeader);
	}
	void HeaderInt(const char *pName, int Value)
	{
		char aHeader[256];
		str_format(aHeader, sizeof(aHeader), "%s: %d", pName, Value);
		Header(aHeader);
	}

	const char *Dest()
	{
		if(m_WriteToFile)
		{
			return m_aDest;
		}
		else
		{
			return nullptr;
		}
	}

	double Current() const { return m_Current.load(std::memory_order_relaxed); }
	double Size() const { return m_Size.load(std::memory_order_relaxed); }
	int Progress() const { return m_Progress.load(std::memory_order_relaxed); }
	EHttpState State() const { return m_State; }
	bool Done() const
	{
		EHttpState State = m_State;
		return State != EHttpState::QUEUED && State != EHttpState::RUNNING;
	}
	void Abort() { m_Abort = true; }
	// If `ValidateBeforeOverwrite` is set, this needs to be called after
	// validating that the downloaded file has the correct format.
	//
	// If called with `true`, it'll place the downloaded file at the final
	// destination, if called with `false`, it'll instead delete the
	// temporary downloaded file.
	void OnValidation(bool Success);

	void Wait();

	void Result(unsigned char **ppResult, size_t *pResultLength) const;
	json_value *ResultJson() const;
	const SHA256_DIGEST &ResultSha256() const;

	int StatusCode() const;
	std::optional<int64_t> ResultAgeSeconds() const;
	std::optional<int64_t> ResultLastModified() const;
};

inline std::unique_ptr<CHttpRequest> HttpHead(const char *pUrl)
{
	auto pResult = std::make_unique<CHttpRequest>(pUrl);
	pResult->Head();
	return pResult;
}

inline std::unique_ptr<CHttpRequest> HttpGet(const char *pUrl)
{
	return std::make_unique<CHttpRequest>(pUrl);
}

inline std::unique_ptr<CHttpRequest> HttpGetFile(const char *pUrl, IStorage *pStorage, const char *pOutputFile, int StorageType)
{
	std::unique_ptr<CHttpRequest> pResult = HttpGet(pUrl);
	pResult->WriteToFile(pStorage, pOutputFile, StorageType);
	pResult->Timeout(CTimeout{4000, 0, 500, 5});
	return pResult;
}

inline std::unique_ptr<CHttpRequest> HttpGetBoth(const char *pUrl, IStorage *pStorage, const char *pOutputFile, int StorageType)
{
	std::unique_ptr<CHttpRequest> pResult = HttpGet(pUrl);
	pResult->WriteToFileAndMemory(pStorage, pOutputFile, StorageType);
	pResult->Timeout(CTimeout{4000, 0, 500, 5});
	return pResult;
}

inline std::unique_ptr<CHttpRequest> HttpPost(const char *pUrl, const unsigned char *pData, size_t DataLength)
{
	auto pResult = std::make_unique<CHttpRequest>(pUrl);
	pResult->Post(pData, DataLength);
	pResult->Timeout(CTimeout{4000, 15000, 500, 5});
	return pResult;
}

inline std::unique_ptr<CHttpRequest> HttpPostJson(const char *pUrl, const char *pJson)
{
	auto pResult = std::make_unique<CHttpRequest>(pUrl);
	pResult->PostJson(pJson);
	pResult->Timeout(CTimeout{4000, 15000, 500, 5});
	return pResult;
}

void EscapeUrl(char *pBuf, int Size, const char *pStr);

template<int N>
void EscapeUrl(char (&aBuf)[N], const char *pStr)
{
	EscapeUrl(aBuf, N, pStr);
}

bool HttpHasIpresolveBug();

// In an ideal world this would be a kernel interface
class CHttp : public IHttp
{
	enum EState
	{
		UNINITIALIZED,
		RUNNING,
		ERROR,
	};

	void *m_pThread = nullptr;

	std::mutex m_Lock{};
	std::condition_variable m_Cv{};
	std::atomic<EState> m_State = UNINITIALIZED;
	std::deque<std::shared_ptr<CHttpRequest>> m_PendingRequests{};
	std::unordered_map<void *, std::shared_ptr<CHttpRequest>> m_RunningRequests{}; // void * == CURL *
	std::chrono::milliseconds m_ShutdownDelay{};
	std::optional<std::chrono::time_point<std::chrono::steady_clock>> m_ShutdownTime{};
	std::atomic<bool> m_Shutdown = false;

	// Only to be used with curl_multi_wakeup
	void *m_pMultiH = nullptr; // void * == CURLM *

	static void ThreadMain(void *pUser);
	void RunLoop();

public:
	// Startup
	bool Init(std::chrono::milliseconds ShutdownDelay);

	// User
	virtual void Run(std::shared_ptr<IHttpRequest> pRequest) override;
	void Shutdown() override;
	~CHttp();
};

#endif // ENGINE_SHARED_HTTP_H
