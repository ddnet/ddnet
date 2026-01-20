#ifndef ENGINE_HTTP_H
#define ENGINE_HTTP_H

#include "kernel.h"

#include <base/hash_ctxt.h>
#include <base/types.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

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

class CTimeout
{
public:
	long m_ConnectTimeoutMs;
	long m_TimeoutMs;
	long m_LowSpeedLimit;
	long m_LowSpeedTime;
};

class IHttpRequest
{
	friend class IHttp;

public:
	IHttpRequest(const char *pUrl);
	virtual ~IHttpRequest();

	void Timeout(CTimeout Timeout) { m_Timeout = Timeout; }
	// Skip the download if the local file is newer or as new as the remote file.
	void MaxResponseSize(int64_t MaxResponseSize) { m_MaxResponseSize = MaxResponseSize; }
	void LogProgress(HTTPLOG LogProgress) { m_LogProgress = LogProgress; }
	void SkipByFileTime(bool SkipByFileTime) { m_SkipByFileTime = SkipByFileTime; }
	void IpResolve(IPRESOLVE IpResolve) { m_IpResolve = IpResolve; }
	void FailOnErrorStatus(bool FailOnErrorStatus) { m_FailOnErrorStatus = FailOnErrorStatus; }
	// Download to memory only. Get the result via `Result*`.
	void WriteToMemory();
	// Download to filesystem and memory.
	void WriteToFileAndMemory(IStorage *pStorage, const char *pDest, int StorageType);
	// Download to the filesystem only.
	void WriteToFile(IStorage *pStorage, const char *pDest, int StorageType);
	// Don't place the file in the specified location until
	// `OnValidation(true)` has been called.
	void ValidateBeforeOverwrite(bool ValidateBeforeOverwrite) { m_ValidateBeforeOverwrite = ValidateBeforeOverwrite; }
	void ExpectSha256(const SHA256_DIGEST &Sha256) { m_ExpectedSha256 = Sha256; }

	void Head();
	void Post(const unsigned char *pData, size_t DataLength);
	void PostJson(const char *pJson);

	virtual void Header(const char *pNameColonValue) = 0;
	void HeaderString(const char *pName, const char *pValue);
	void HeaderInt(const char *pName, int Value);

	const char *Dest() const;
	double Current() const { return m_Current.load(std::memory_order_relaxed); }
	double Size() const { return m_Size.load(std::memory_order_relaxed); }
	int Progress() const { return m_Progress.load(std::memory_order_relaxed); }
	EHttpState State() const { return m_State; }
	bool Done() const
	{
		EHttpState CurrentState = State();
		return CurrentState != EHttpState::QUEUED && CurrentState != EHttpState::RUNNING;
	}
	virtual void Abort() { m_Abort = true; }
	bool IsAbortRequested() const { return m_Abort; }
	void Wait();

	/**
	 * Callback functions for handling progress of an HTTP request.
	 *
	 * @remark These callbacks may be called from a separate HTTP thread.
	 *         Make sure the implementation is thread-safe and **do not stall the thread!**
	 */
	class IProgressCallback
	{
	public:
		virtual ~IProgressCallback() = default;
		virtual void OnProgress() = 0;
		virtual void OnCompletion(EHttpState State) = 0;
	};
	void SetProgressCallback(IProgressCallback *pCallback) { m_pProgressCallback = pCallback; }

	// If `ValidateBeforeOverwrite` is set, this needs to be called after
	// validating that the downloaded file has the correct format.
	//
	// If called with `true`, it'll place the downloaded file at the final
	// destination, if called with `false`, it'll instead delete the
	// temporary downloaded file.
	void OnValidation(bool Success);

	void Result(unsigned char **ppResult, size_t *pResultLength) const;
	json_value *ResultJson() const;
	const SHA256_DIGEST &ResultSha256() const;

	int StatusCode() const;
	std::optional<int64_t> ResultAgeSeconds() const;
	std::optional<int64_t> ResultLastModified() const;

protected:
	static const char *const USER_AGENT_STRING;
	enum class REQUEST
	{
		GET,
		HEAD,
		POST,
		POST_JSON,
	};
	static const char *GetRequestType(REQUEST Type);

	// Request
	char m_aUrl[256] = "";
	REQUEST m_Type = REQUEST::GET;
	unsigned char *m_pBody = nullptr;
	size_t m_BodyLength = 0;

	// Settings
	CTimeout m_Timeout = CTimeout{0, 0, 0, 0};
	int64_t m_MaxResponseSize = -1;
	HTTPLOG m_LogProgress = HTTPLOG::ALL;
	bool m_SkipByFileTime = true;
	IPRESOLVE m_IpResolve = IPRESOLVE::WHATEVER;
	bool m_FailOnErrorStatus = true;
	bool m_ValidateBeforeOverwrite = false;
	std::optional<SHA256_DIGEST> m_ExpectedSha256 = std::nullopt;
	int64_t m_IfModifiedSince = -1;

	// Result
	std::optional<SHA256_DIGEST> m_ActualSha256 = std::nullopt;
	SHA256_CTX m_ActualSha256Ctx;
	uint64_t m_ResponseLength = 0;
	int m_StatusCode = 0;
	std::optional<int64_t> m_ResultDate = std::nullopt;
	std::optional<int64_t> m_ResultLastModified = std::nullopt;

	bool m_WriteToMemory = true;
	bool m_WriteToFile = false;

	// If `m_WriteToMemory` is true.
	size_t m_BufferSize = 0;
	unsigned char *m_pBuffer = nullptr;

	// If `m_WriteToFile` is true.
	IOHANDLE m_File = nullptr;
	char m_aDestAbsoluteTmp[IO_MAX_PATH_LENGTH] = "";
	char m_aDestAbsolute[IO_MAX_PATH_LENGTH] = "";
	char m_aDest[IO_MAX_PATH_LENGTH] = "";

	// Progress
	std::atomic<double> m_Size = 0.0;
	std::atomic<double> m_Current = 0.0;
	std::atomic<int> m_Progress = 0;
	std::atomic<EHttpState> m_State = EHttpState::QUEUED;
	std::atomic<bool> m_Abort = false;
	IProgressCallback *m_pProgressCallback = nullptr;

	std::mutex m_WaitMutex;
	std::condition_variable m_WaitCondition;

	bool ShouldSkipRequest();
	// Abort the request with an error if `BeforeInit()` returns false.
	bool BeforeInit();

	// Abort the request if `OnData()` returns something other than
	// `DataSize`.
	size_t OnData(const char *pData, size_t DataSize);
	void OnCompletionInternal(EHttpState State);
};

std::unique_ptr<IHttpRequest> CreateHttpRequest(const char *pUrl);

inline std::unique_ptr<IHttpRequest> HttpHead(const char *pUrl)
{
	std::unique_ptr<IHttpRequest> pResult = CreateHttpRequest(pUrl);
	pResult->Head();
	return pResult;
}

inline std::unique_ptr<IHttpRequest> HttpGet(const char *pUrl)
{
	return CreateHttpRequest(pUrl);
}

inline std::unique_ptr<IHttpRequest> HttpGetFile(const char *pUrl, IStorage *pStorage, const char *pOutputFile, int StorageType)
{
	std::unique_ptr<IHttpRequest> pResult = HttpGet(pUrl);
	pResult->WriteToFile(pStorage, pOutputFile, StorageType);
	pResult->Timeout(CTimeout{4000, 0, 500, 5});
	return pResult;
}

inline std::unique_ptr<IHttpRequest> HttpGetBoth(const char *pUrl, IStorage *pStorage, const char *pOutputFile, int StorageType)
{
	std::unique_ptr<IHttpRequest> pResult = HttpGet(pUrl);
	pResult->WriteToFileAndMemory(pStorage, pOutputFile, StorageType);
	pResult->Timeout(CTimeout{4000, 0, 500, 5});
	return pResult;
}

inline std::unique_ptr<IHttpRequest> HttpPost(const char *pUrl, const unsigned char *pData, size_t DataLength)
{
	std::unique_ptr<IHttpRequest> pResult = CreateHttpRequest(pUrl);
	pResult->Post(pData, DataLength);
	pResult->Timeout(CTimeout{4000, 15000, 500, 5});
	return pResult;
}

inline std::unique_ptr<IHttpRequest> HttpPostJson(const char *pUrl, const char *pJson)
{
	std::unique_ptr<IHttpRequest> pResult = CreateHttpRequest(pUrl);
	pResult->PostJson(pJson);
	pResult->Timeout(CTimeout{4000, 15000, 500, 5});
	return pResult;
}

void EscapeUrl(char *pBuffer, size_t BufferSize, const char *pUrl);

template<size_t BufferSize>
void EscapeUrl(char (&aBuffer)[BufferSize], const char *pUrl)
{
	EscapeUrl(aBuffer, BufferSize, pUrl);
}

class IHttp : public IInterface
{
	MACRO_INTERFACE("http")

public:
	virtual void Run(std::shared_ptr<IHttpRequest> pRequest) = 0;

	virtual bool HasIpresolveBug() const = 0;
};

class IEngineHttp : public IHttp
{
	MACRO_INTERFACE("enginehttp")

public:
	virtual bool Init(std::chrono::milliseconds ShutdownDelay) = 0;
	void Shutdown() override = 0;
};

IEngineHttp *CreateEngineHttp();

#endif
