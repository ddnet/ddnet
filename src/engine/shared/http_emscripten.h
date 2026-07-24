#ifndef ENGINE_SHARED_HTTP_EMSCRIPTEN_H
#define ENGINE_SHARED_HTTP_EMSCRIPTEN_H

#include <base/detect.h>
#if defined(CONF_PLATFORM_EMSCRIPTEN)

#include <engine/http.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

typedef struct emscripten_fetch_t emscripten_fetch_t;

class CHttpEmscripten;

class CHttpRequestEmscripten : public IHttpRequest
{
	friend CHttpEmscripten;

public:
	CHttpRequestEmscripten(const char *pUrl);
	~CHttpRequestEmscripten() override;

	void Header(const char *pNameColonValue) override;

	void Abort() override;

private:
	CHttpEmscripten *m_pHttp;

	std::vector<std::pair<std::string, std::string>> m_vRequestHeaders;

	emscripten_fetch_t *m_pFetch = nullptr;
	bool m_CallbackFinished = false;

	bool ConfigureAndRun();
	void OnSuccess();
	void OnFailure();
	void OnProgress();
	void OnHeader(const char *pName, const char *pValue);
	void OnCompletionInternal(EHttpState State, const char *pErrorDetail);

	static void FetchCallbackSuccess(emscripten_fetch_t *pFetch);
	static void FetchCallbackFailure(emscripten_fetch_t *pFetch);
	static void FetchCallbackProgress(emscripten_fetch_t *pFetch);
};

class CHttpEmscripten : public IEngineHttp
{
	friend CHttpRequestEmscripten;

public:
	// Lifecycle
	bool Init(std::chrono::milliseconds ShutdownDelay) override;
	void Shutdown() override;
	~CHttpEmscripten();

	// User
	void Run(std::shared_ptr<IHttpRequest> pRequest) override;
	bool HasIpresolveBug() const override;

private:
	void *m_pThread = nullptr;

	std::mutex m_Lock;
	std::condition_variable m_ConditionVariableInit;
	std::condition_variable m_ConditionVariableLoop;
	std::atomic<bool> m_Initialized = false;
	std::deque<std::shared_ptr<CHttpRequestEmscripten>> m_PendingRequests;
	std::unordered_map<emscripten_fetch_t *, EHttpState> m_PendingFetchChanges;
	std::unordered_map<emscripten_fetch_t *, std::shared_ptr<CHttpRequestEmscripten>> m_RunningRequests;
	std::chrono::milliseconds m_ShutdownDelay{};
	std::optional<std::chrono::time_point<std::chrono::steady_clock>> m_ShutdownTime;
	std::atomic<bool> m_Shutdown = false;
	std::atomic<bool> m_StartedShutdown = false;

	static void ThreadMain(void *pUser);
	void RunLoop();
	void AddPendingStateChange(emscripten_fetch_t *pFetch, EHttpState State);
};

#endif // CONF_PLATFORM_EMSCRIPTEN
#endif // ENGINE_SHARED_HTTP_EMSCRIPTEN_H
